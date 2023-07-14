#pragma once

#include <cassert>
#include <initializer_list>
#include <stdexcept>

#include "array_ptr.h"

class ReserveProxyObj {
 public:
  ReserveProxyObj() = default;
  ReserveProxyObj(size_t new_capacity) : capacity_(new_capacity){};
  size_t GetCapacity() { return capacity_; }

 private:
  size_t capacity_ = 0;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
  return ReserveProxyObj(capacity_to_reserve);
};

template <typename Type>
class SimpleVector {
 public:
  using Iterator = Type*;
  using ConstIterator = const Type*;

  SimpleVector() noexcept = default;

  // Создаёт вектор из size элементов, инициализированных значением по умолчанию
  explicit SimpleVector(size_t size) : SimpleVector(size, std::move(Type{})) {}

  // Создаёт вектор из size элементов, инициализированных значением value
  SimpleVector(size_t size, const Type& value)
      : items_(size), size_(size), capacity_(size) {
    std::fill(begin(), end(), value);
  }

  SimpleVector(size_t size, Type&& value)
      : items_(size), size_(size), capacity_(size) {
    for (auto it = items_.Get(); it != items_.Get() + size; ++it) {
      *it = std::move(value);
      value = Type{};  // Обновляем значение переменной на значение по умолчанию
                       // (перемещение)
    }
  }

  // Создаёт вектор из std::initializer_list
  SimpleVector(std::initializer_list<Type> init)
      : items_(init.size()), size_(init.size()), capacity_(init.size()) {
    std::move(init.begin(), init.end(), items_.Get());
  }

  SimpleVector(const SimpleVector& other) : items_(other.GetSize()) {
    size_ = other.GetSize();
    capacity_ = other.GetSize();
    std::copy(other.begin(), other.end(), begin());
  }

  SimpleVector(SimpleVector&& other) noexcept : items_(other.size_) {
    if (this != &other) {
      items_.swap(other.items_);
      size_ = std::move(other.size_);
      capacity_ = std::move(other.capacity_);
      other.Clear();  // Очистка вектора после перемещения
    }
  }

  SimpleVector(ReserveProxyObj obj)
      : items_(obj.GetCapacity()), size_(0), capacity_(obj.GetCapacity()) {}

  SimpleVector& operator=(const SimpleVector& rhs) {
    if (this != &rhs) {
      if (rhs.IsEmpty())
        Clear();
      else {
        SimpleVector copy(rhs);
        swap(copy);
      }
    }
    return *this;
  }

  // Возвращает количество элементов в массиве
  size_t GetSize() const noexcept { return size_; }

  // Возвращает вместимость массива
  size_t GetCapacity() const noexcept { return capacity_; }

  // Сообщает, пустой ли массив
  bool IsEmpty() const noexcept { return size_ == 0; }

  // Возвращает ссылку на элемент с индексом index
  Type& operator[](size_t index) noexcept { return items_[index]; }

  // Возвращает константную ссылку на элемент с индексом index
  const Type& operator[](size_t index) const noexcept { return items_[index]; }

  // Возвращает константную ссылку на элемент с индексом index
  // Выбрасывает исключение std::out_of_range, если index >= size
  Type& At(size_t index) {
    if (index >= size_) {
      throw std::out_of_range("Error: no index");
    }
    return items_[index];
  }

  // Возвращает константную ссылку на элемент с индексом index
  // Выбрасывает исключение std::out_of_range, если index >= size
  const Type& At(size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("Error: no index");
    }
    return items_[index];
  }

  // Обнуляет размер массива, не изменяя его вместимость
  void Clear() noexcept { size_ = 0; }

  // Изменяет размер массива.
  // При увеличении размера новые элементы получают значение по умолчанию для
  // типа Type
  void Resize(size_t new_size) {
    if (new_size < size_ || new_size == size_) {
      size_ = new_size;
    }
    if (new_size <= capacity_) {
      for (int i = size_; i < new_size; ++i) {
        items_[i] = Type();
      }
      size_ = new_size;
    }
    if (new_size > capacity_) {
      ChangeCapacity(new_size);
      size_ = new_size;
    }
  }

  void PushBack(const Type& item) {
    if (size_ == capacity_) {
      size_ != 0 ? ChangeCapacity(size_ * 2) : ChangeCapacity(1);
    }
    items_[size_] = item;
    ++size_;
  }
  void PushBack(Type&& item) {
    if (size_ == capacity_) {
      size_ != 0 ? ChangeCapacity(size_ * 2) : ChangeCapacity(1);
    }
    items_[size_] = std::move(item);  // move item in items_[size_]
    item = std::move(
        Type{});  // освобождение item(создаёт временный объект Type{}, а затем
                  // его содержимает перемещается в item)
    size_++;
  }

  // Вставляет значение value в позицию pos.
  // Возвращает итератор на вставленное значение
  // Если перед вставкой значения вектор был заполнен полностью,
  // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0
  // стать равной 1
  Iterator Insert(ConstIterator pos, const Type& value) {
    assert(pos >= begin() && pos <= end());
    auto index = std::distance(cbegin(), pos);
    if (size_ == capacity_) {
      size_ != 0 ? ChangeCapacity(size_ * 2) : ChangeCapacity(1);
    }
    auto it = begin() + index;
    std::copy_backward(it, end(), end() + 1);
    items_[index] = value;
    ++size_();
    return Iterator(begin() + index);
  }

  Iterator Insert(ConstIterator pos, Type&& value) {
    assert(pos >= begin() && pos <= end());
    auto index =
        std::distance(cbegin(), pos);  // кол-во элементов от начала до pos
    if (size_ == capacity_)
      size_ != 0
          ? ChangeCapacity(2 * size_)
          : ChangeCapacity(1);  // если размер массива не 0, то увеличить его
                                // вместимость на size * 2, иначе вместимость 1
    auto it =
        begin() + index;  // итератор на позицию вставки. Сдвигает итератор
                          // begin на нужное кол-во элементов index
    std::move_backward(
        it, end(), end() + 1);  // сдвиг элементов от конца массива до it вправо
    items_[index] = std::move(value);  // move value in items_[index]
    ++size_;
    value = std::move(Type{});  // освобождение value
    return Iterator(begin() + index);
  }

  void PopBack() noexcept {
    if (IsEmpty()) return;
    --size_;
  }

  Iterator Erase(ConstIterator pos) {
    assert(pos >= begin() && pos < end());
    auto index = std::distance(cbegin(), pos);
    auto it = begin() + index;
    std::move((it + 1), end(), it);
    --size_;
    return Iterator(pos);
  }

  void swap(SimpleVector& other) noexcept {
    items_.swap(other.items_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
  }

  // Обменивает значение с другим вектором
  void swap(SimpleVector&& other) noexcept {
    items_.swap(other.items_);
    std::swap(capacity_, other.capacity_);
    std::swap(size_, other.size_);
    other.Clear();
  }

  void Reserve(size_t new_capacity) {
    if (capacity_ < new_capacity) {
      ChangeCapacity(new_capacity);
    }
  }

  // Возвращает итератор на начало массива
  Iterator begin() noexcept { return &items_[0]; }

  // Возвращает итератор на элемент, следующий за последним
  Iterator end() noexcept { return &items_[size_]; }

  // Возвращает константный итератор на начало массива
  ConstIterator begin() const noexcept { return &items_[0]; }

  // Возвращает итератор на элемент, следующий за последним
  ConstIterator end() const noexcept { return end(); }

  // Возвращает константный итератор на начало массива
  ConstIterator cbegin() const noexcept { return cbegin(); }

  // Возвращает итератор на элемент, следующий за последним
  ConstIterator cend() const noexcept { return end(); }

 private:
  ArrayPtr<Type> items_;
  size_t size_ = 0;
  size_t capacity_ = 0;

  void ChangeCapacity(const size_t new_size) {
    SimpleVector new_arr(new_size);
    size_t prev_size = size_;
    std::move(begin(), end(), new_arr.begin());
    swap(std::move(new_arr));
    size_ = prev_size;
  }
};

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs,
                       const SimpleVector<Type>& rhs) {
  return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs,
                       const SimpleVector<Type>& rhs) {
  return !(lhs == rhs);
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs,
                      const SimpleVector<Type>& rhs) {
  return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                                      rhs.cend());
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs,
                       const SimpleVector<Type>& rhs) {
  return !(rhs < lhs);
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs,
                      const SimpleVector<Type>& rhs) {
  return rhs < lhs;
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs,
                       const SimpleVector<Type>& rhs) {
  return !(rhs > lhs);
}
