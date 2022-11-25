// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_

#include <bitset>
#include <cstddef>
#include <iterator>
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"

namespace autofill {

// A set container with a std::set<T>-like interface for integral or enum types
// T that have a dense and small representation as unsigned integers.
//
// The order of the elements in the container corresponds to their integer
// representation.
//
// The lower and upper bounds of elements storable in a container are
// [T(0), kMaxValue]. By default, kMaxValue is T::kMaxValue.
//
// Internally, the set is represented as a std::bitset.
//
// Time and space complexity depend on std::bitset:
// - insert(), erase(), contains() should run in time O(1)
// - empty(), size(), iteration should run in time O(kMaxValue)
// - sizeof(DenseSet) should be ceil(kMaxValue / 8) bytes.
//
// Iterators are invalidated when the owning container is destructed or moved,
// or when the element the iterator points to is erased from the container.
//
// If `packed` is true, the smallest sufficient raw integer is used to represent
// the underlying bitset. Otherwise, or if more than 64 bits are needed, it uses
// std::bitset.
template <typename T, T kMaxValue = T::kMaxValue, bool packed = true>
class DenseSet {
 private:
  using Index = std::make_unsigned_t<T>;

  // The maximum supported bit index. Indexing starts at 0, so kMaxBitIndex ==
  // 63 means we need 64 bits.
  static constexpr size_t kMaxBitIndex = base::checked_cast<Index>(kMaxValue);

 public:
  // A bidirectional iterator for the DenseSet.
  class Iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = T;

    constexpr Iterator() = default;

    friend bool operator==(const Iterator& a, const Iterator& b) {
      DCHECK(a.owner_);
      DCHECK_EQ(a.owner_, b.owner_);
      return a.index_ == b.index_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

    T operator*() const {
      DCHECK(derefenceable());
      return index_to_value(index_);
    }

    Iterator& operator++() {
      ++index_;
      Skip(kForward);
      return *this;
    }

    Iterator operator++(int) {
      auto that = *this;
      operator++();
      return that;
    }

    Iterator& operator--() {
      --index_;
      Skip(kBackward);
      return *this;
    }

    Iterator operator--(int) {
      auto that = *this;
      operator--();
      return that;
    }

   private:
    friend DenseSet;

    enum Direction { kBackward = -1, kForward = 1 };

    constexpr Iterator(const DenseSet* owner, Index index)
        : owner_(owner), index_(index) {}

    // Advances the index, starting from the current position, to the next
    // non-empty one. std::bitset does not offer a find-next-set operation.
    void Skip(Direction direction) {
      DCHECK_LE(index_, owner_->max_size());
      while (index_ < owner_->max_size() && !derefenceable()) {
        index_ += direction;
      }
    }

    bool derefenceable() const {
      DCHECK_LT(index_, owner_->max_size());
      return owner_->bitset_.test(index_);
    }

    const DenseSet* owner_ = nullptr;

    // The current index is in the interval [0, owner_->max_size()].
    Index index_ = 0;
  };

  using value_type = T;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr DenseSet() = default;

  // The `constexpr` constructor allows for compile-time initialization of
  // DenseSets. This only works if the set fits into 64 bits. Otherwise, we
  // fall back to a non-`constexpr` constructor.

  template <size_t kMaxBitIndex = kMaxBitIndex,
            std::enable_if_t<(kMaxBitIndex < 64), bool> = true>
  constexpr DenseSet(std::initializer_list<T> init)
      : bitset_(initializer_list_to_bitmask(init)) {}

  template <size_t kMaxBitIndex = kMaxBitIndex,
            std::enable_if_t<(kMaxBitIndex >= 64), bool> = true>
  DenseSet(std::initializer_list<T> init) {
    for (const auto& x : init) {
      insert(x);
    }
  }

  template <typename InputIt>
  DenseSet(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  // Converts the bitset back to a raw bitmask. Useful for serialization.
  template <size_t kMaxBitIndex = kMaxBitIndex,
            std::enable_if_t<(kMaxBitIndex < 64), bool> = true>
  uint64_t to_uint64() const {
    return bitset_.to_ullong();
  }

  friend bool operator==(const DenseSet& a, const DenseSet& b) {
    return a.bitset_ == b.bitset_;
  }

  friend bool operator!=(const DenseSet& a, const DenseSet& b) {
    return !(a == b);
  }

  // Iterators.

  // Returns an iterator to the beginning.
  iterator begin() const {
    const_iterator it(this, 0);
    it.Skip(Iterator::kForward);
    return it;
  }
  const_iterator cbegin() const { return begin(); }

  // Returns an iterator to the end.
  iterator end() const { return iterator(this, max_size()); }
  const_iterator cend() const { return end(); }

  // Returns a reverse iterator to the beginning.
  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return rbegin(); }

  // Returns a reverse iterator to the end.
  reverse_iterator rend() const { return reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return rend(); }

  // Capacity.

  // Returns true if the set is empty, otherwise false.
  bool empty() const { return bitset_.none(); }

  // Returns the number of elements the set has.
  size_t size() const { return bitset_.count(); }

  // Returns the maximum number of elements the set can have.
  constexpr size_t max_size() const { return kMaxBitIndex + 1; }

  // Modifiers.

  // Clears the contents.
  void clear() { bitset_.reset(); }

  // Inserts value |x| if it is not present yet, and returns an iterator to the
  // inserted or existing element and a boolean that indicates whether the
  // insertion took place.
  std::pair<iterator, bool> insert(T x) {
    bool contained = contains(x);
    bitset_.set(value_to_index(x));
    return {find(x), !contained};
  }

  // Inserts all values of |xs| into the present set.
  void insert_all(const DenseSet& xs) { bitset_ |= xs.bitset_; }

  // Erases the element whose index matches the index of |x| and returns the
  // number of erased elements (0 or 1).
  size_t erase(T x) {
    bool contained = contains(x);
    bitset_.reset(value_to_index(x));
    return contained ? 1 : 0;
  }

  // Erases the element |*it| and returns an iterator to its successor.
  iterator erase(const_iterator it) {
    DCHECK(it.owner_ == this && it.derefenceable());
    bitset_.reset(it.index_);
    it.Skip(const_iterator::kForward);
    return it;
  }

  // Erases the elements [first,last) and returns |last|.
  iterator erase(const_iterator first, const_iterator last) {
    DCHECK(first.owner_ == this && last.owner_ == this);
    while (first != last) {
      bitset_.reset(first.index_);
      ++first;
    }
    return last;
  }

  // Erases all values of |xs| into the present set.
  void erase_all(const DenseSet& xs) { bitset_ &= ~xs.bitset_; }

  // Lookup.

  // Returns 1 if |x| is an element, otherwise 0.
  size_t count(T x) const { return contains(x) ? 1 : 0; }

  // Returns an iterator to the element |x| if it exists, otherwise end().
  const_iterator find(T x) const {
    return contains(x) ? const_iterator(this, value_to_index(x)) : cend();
  }

  // Returns true if |x| is an element, else |false|.
  bool contains(T x) const { return bitset_.test(value_to_index(x)); }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_none(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_).none();
  }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_any(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_).any();
  }

  // Returns true if every elements of |xs| is an element, else |false|.
  bool contains_all(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) == xs.bitset_;
  }

  // Returns an iterator to the first element not less than the |x|, or end().
  const_iterator lower_bound(T x) const {
    const_iterator it(this, value_to_index(x));
    it.Skip(Iterator::kForward);
    return it;
  }

  // Returns an iterator to the first element greater than |x|, or end().
  const_iterator upper_bound(T x) const {
    const_iterator it(this, value_to_index(x) + 1);
    it.Skip(Iterator::kForward);
    return it;
  }

 private:
  friend Iterator;

  // The default implementation for the underlying bitset forwards to
  // std::bitset. For bitsets up to 64 bits, there's a specialization.
  template <size_t kMaxBitIndex, typename Enable = void>
  class BitSet : public std::bitset<kMaxBitIndex + 1> {
   public:
    using std::bitset<kMaxBitIndex + 1>::bitset;
  };

  // Specialization that uses the smallest fundamental integer type that can
  // hold `kMaxBitIndex + 1` bits (note that kMaxBitIndex is an index).
  // Only enabled if `packed` is true.
  template <size_t kMaxBitIndex>
  class BitSet<kMaxBitIndex, std::enable_if_t<(packed && kMaxBitIndex < 64)>> {
   public:
    using bitmask_type = std::conditional_t<
        (kMaxBitIndex < 8),
        uint8_t,
        std::conditional_t<
            (kMaxBitIndex < 16),
            uint16_t,
            std::conditional_t<(kMaxBitIndex < 32), uint32_t, uint64_t>>>;

    constexpr explicit BitSet(bitmask_type bitmask = 0) : bitmask_(bitmask) {}

    constexpr uint64_t to_ullong() const { return bitmask_; }

    friend constexpr bool operator==(BitSet lhs, BitSet rhs) {
      return lhs.bitmask_ == rhs.bitmask_;
    }

    friend constexpr bool operator!=(BitSet lhs, BitSet rhs) {
      return !(lhs == rhs);
    }

    friend constexpr BitSet operator&(BitSet lhs, BitSet rhs) {
      return BitSet(lhs.bitmask_ & rhs.bitmask_);
    }

    friend constexpr BitSet& operator&=(BitSet& lhs, BitSet rhs) {
      lhs.bitmask_ &= rhs.bitmask_;
      return lhs;
    }

    friend constexpr BitSet& operator|=(BitSet& lhs, BitSet rhs) {
      lhs.bitmask_ |= rhs.bitmask_;
      return lhs;
    }

    constexpr BitSet operator~() const { return BitSet(~bitmask_); }

    constexpr bool none() const { return bitmask_ == 0; }
    constexpr bool any() const { return bitmask_ != 0; }

    constexpr size_t count() const {
      // Compiles to a POPCOUNT instruction. Could be replaced with
      // std::popcount() in C++20.
      return std::bitset<kMaxBitIndex + 1>(bitmask_).count();
    }

    constexpr bool test(size_t i) const {
      DCHECK_LE(i, kMaxBitIndex);
      return bitmask_ & (1ULL << i);
    }

    constexpr void set(size_t i) {
      DCHECK_LE(i, kMaxBitIndex);
      bitmask_ |= 1ULL << i;
    }

    constexpr void reset(size_t i) {
      DCHECK_LE(i, kMaxBitIndex);
      bitmask_ &= ~(1ULL << i);
    }

    constexpr void reset() { bitmask_ = 0; }

   private:
    bitmask_type bitmask_ = 0;
  };

  // Needed to use std::conditional_t.
  struct Wrapper {
    using type = T;
  };

  static constexpr Index value_to_index(T x) {
    DCHECK(index_to_value(0) <= x && x <= kMaxValue);
    return base::checked_cast<Index>(x);
  }

  static constexpr T index_to_value(Index i) {
    DCHECK_LE(i, base::checked_cast<Index>(kMaxValue));
    using UnderlyingType =
        typename std::conditional_t<std::is_enum<T>::value,
                                    std::underlying_type<T>, Wrapper>::type;
    return static_cast<T>(base::checked_cast<UnderlyingType>(i));
  }

  // Helper for `constexpr DenseSet(std::initializer_list<T>)`.
  //
  // While std::bitset's constructor takes an `unsigned long long`, we use
  // `uint64_t` because Chromium bans `unsigned long long`. Both are 64 bit
  // integers, so they're interchangeable.
  static constexpr uint64_t initializer_list_to_bitmask(
      const std::initializer_list<T>& init) {
    uint64_t bitmask = 0;
    for (const auto& x : init) {
      bitmask |= 1ULL << value_to_index(x);
    }
    return bitmask;
  }

  static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "");
  static_assert(0 <= base::checked_cast<Index>(kMaxValue) + 1, "");

  BitSet<kMaxBitIndex> bitset_{};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
