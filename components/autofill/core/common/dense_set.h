// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"

namespace autofill {

namespace internal {

// The number of bits in `T`.
template <typename T>
static constexpr size_t kBitsPer = sizeof(T) * CHAR_BIT;

// A bitset represented as `std::array<Word, kNumWords>.
// There's a specialization further down for `kNumWords == 1`.
template <typename Word, size_t kNumWords>
class Bitset {
 public:
  constexpr Bitset() = default;

  constexpr size_t num_set_bits() const {
    // We count the number of bits in `words_`. DenseSet ensures that all bits
    // beyond `kMaxBitIndex` are zero. This is necessary for size() to be
    // correct.
    size_t num = 0;
    for (const auto word : words_) {
      num += std::popcount(word);
    }
    return num;
  }

  constexpr bool get_bit(size_t index) const {
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    return words_[word] & (static_cast<Word>(1) << bit);
  }

  constexpr void set_bit(size_t index) {
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    words_[word] |= static_cast<Word>(1) << bit;
  }

  constexpr void unset_bit(size_t index) {
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    words_[word] &= ~(static_cast<Word>(1) << bit);
  }

  constexpr Bitset operator|=(const Bitset& rhs) {
    for (size_t i = 0; i < words_.size(); ++i) {
      words_[i] |= rhs.words_[i];
    }
    return *this;
  }

  constexpr Bitset operator&=(const Bitset& rhs) {
    for (size_t i = 0; i < words_.size(); ++i) {
      words_[i] &= rhs.words_[i];
    }
    return *this;
  }

  friend constexpr Bitset operator&(Bitset lhs, const Bitset& rhs) {
    return lhs &= rhs;
  }

  friend constexpr Bitset operator~(Bitset x) {
    for (size_t i = 0; i < x.words_.size(); ++i) {
      x.words_[i] = ~x.words_[i];
    }
    return x;
  }

  friend auto operator<=>(const Bitset& lhs, const Bitset& rhs) = default;
  friend bool operator==(const Bitset& lhs, const Bitset& rhs) = default;

  constexpr base::span<const Word, kNumWords> data() const { return words_; }

 private:
  std::array<Word, kNumWords> words_{};
};

// Specialization that uses a single integer instead of an std::array.
template <typename Word>
class Bitset<Word, 1u> {
 public:
  constexpr Bitset() = default;

  constexpr size_t num_set_bits() const { return std::popcount(word_); }

  constexpr bool get_bit(size_t index) const {
    return word_ & (static_cast<Word>(1) << index);
  }

  constexpr void set_bit(size_t index) {
    word_ |= static_cast<Word>(1) << index;
  }

  constexpr void unset_bit(size_t index) {
    word_ &= ~(static_cast<Word>(1) << index);
  }

  constexpr Bitset operator|=(const Bitset& rhs) {
    word_ |= rhs.word_;
    return *this;
  }

  constexpr Bitset operator&=(const Bitset& rhs) {
    word_ &= rhs.word_;
    return *this;
  }

  friend constexpr Bitset operator&(Bitset lhs, const Bitset& rhs) {
    lhs.word_ &= rhs.word_;
    return lhs;
  }

  friend constexpr Bitset operator~(Bitset x) {
    x.word_ = ~x.word_;
    return x;
  }

  friend constexpr auto operator<=>(const Bitset& lhs,
                                    const Bitset& rhs) = default;
  friend constexpr bool operator==(Bitset lhs, Bitset rhs) = default;

  constexpr base::span<const Word, 1> data() const {
    return base::span_from_ref(word_);
  }

 private:
  Word word_;
};

}  // namespace internal

template <typename T>
struct DenseSetTraits {
  static constexpr T kMinValue = T(0);
  static constexpr T kMaxValue = T::kMaxValue;
  static constexpr bool kPacked = false;
};

// A set container with a std::set<T>-like interface for integral or enum types
// T that have a dense and small representation as unsigned integers.
//
// The order of the elements in the container corresponds to their integer
// representation.
//
// The lower and upper bounds of elements storable in a container are
// [Traits::kMinValue, Traits::kMaxValue]. The default is [T(0), T::kMaxValue].
//
// The `Traits::kPacked` parameter indicates whether the memory consumption of a
// DenseSet object should be minimized. That comes at the cost of slightly
// larger code size.
//
// Time and space complexity:
// - insert(), erase(), contains() run in time O(1)
// - empty(), size(), iteration run in time O(Traits::kMaxValue)
// - sizeof(DenseSet) is, for N = `Traits::kMaxValue - Traits::kMinValue + 1,
//   - if `!Traits::kPacked`: the minimum of {1, 2, 4, 8 * ceil(N / 64)} bytes
//     that has at least N bits;
//   - if `Traits::kPacked`: ceil(N / 8) bytes.
//
// Iterators are invalidated when the owning container is destructed or moved,
// or when the element the iterator points to is erased from the container.
template <typename T, typename Traits = DenseSetTraits<T>>
class DenseSet {
 private:
  static_assert(std::is_integral<T>::value || std::is_enum<T>::value);

  // Needed for std::conditional_t.
  struct Wrapper {
    using type = T;
  };

  // For arithmetic on `T`.
  using UnderlyingType = typename std::conditional_t<std::is_enum<T>::value,
                                                     std::underlying_type<T>,
                                                     Wrapper>::type;

  // The index of a bit in the underlying bitset. Use
  // value_to_index() and index_to_value() for conversion.
  using Index = std::make_unsigned_t<UnderlyingType>;

  // We can't use `base::to_underlying()` because `T` may be not an enum.
  static constexpr UnderlyingType to_underlying(T x) {
    return static_cast<UnderlyingType>(x);
  }

  static_assert(to_underlying(Traits::kMinValue) <=
                to_underlying(Traits::kMaxValue));

  // The maximum supported bit index. Indexing starts at 0, so kMaxBitIndex ==
  // 63 means we need 64 bits. This is a `size_t` to avoid `kMaxBitIndex + 1`
  // from overflowing.
  static constexpr size_t kMaxBitIndex = base::checked_cast<Index>(
      to_underlying(Traits::kMaxValue) - to_underlying(Traits::kMinValue));

  static_assert(kMaxBitIndex <
                std::numeric_limits<decltype(kMaxBitIndex)>::max());

 public:
  // The bitset is represented as array of words.
  using Word = std::conditional_t<
      (Traits::kPacked || kMaxBitIndex < 8),
      uint8_t,
      std::conditional_t<
          (kMaxBitIndex < 16),
          uint16_t,
          std::conditional_t<(kMaxBitIndex < 32), uint32_t, uint64_t>>>;

 private:
  // Returns ceil(x / y).
  static constexpr size_t ceil_div(size_t x, size_t y) {
    return (x + y - 1) / y;
  }

 public:
  // The number of `Word`s needed to hold `kMaxBitIndex + 1` bits.
  static constexpr size_t kNumWords =
      ceil_div(kMaxBitIndex + 1, internal::kBitsPer<Word>);

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
      DCHECK(dereferenceable());
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
    // non-empty one.
    void Skip(Direction direction) {
      DCHECK_LE(index_, owner_->max_size());
      while (index_ < owner_->max_size() && !dereferenceable()) {
        index_ += direction;
      }
    }

    bool dereferenceable() const {
      DCHECK_LT(index_, owner_->max_size());
      return owner_->bitset_.get_bit(index_);
    }

    raw_ptr<const DenseSet<T, Traits>> owner_ = nullptr;

    // The current index is in the interval [0, owner_->max_size()].
    Index index_ = 0;
  };

  using value_type = T;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr DenseSet() = default;

  constexpr DenseSet(std::initializer_list<T> init) {
    for (const auto& x : init) {
      bitset_.set_bit(value_to_index(x));
    }
  }

  template <typename InputIt, typename Proj = std::identity>
    requires(std::input_iterator<InputIt>)
  constexpr DenseSet(InputIt first, InputIt last, Proj proj = {}) {
    for (auto it = first; it != last; ++it) {
      insert(std::invoke(proj, *it));
    }
  }

  template <typename Range, typename Proj = std::identity>
    requires(std::ranges::input_range<Range>)
  constexpr explicit DenseSet(const Range& range, Proj proj = {})
      : DenseSet(std::ranges::begin(range), std::ranges::end(range), proj) {}

  // Returns a set containing all values from `kMinValue` to `kMaxValue`,
  // regardless of whether the values represent an existing enum.
  static constexpr DenseSet all() {
    DenseSet set;
    for (Index x = value_to_index(Traits::kMinValue);
         x <= value_to_index(Traits::kMaxValue); ++x) {
      set.insert(index_to_value(x));
    }
    return set;
  }

  // Returns a raw bitmask. Useful for serialization.
  constexpr base::span<const Word, kNumWords> data() const {
    return bitset_.data();
  }

  friend auto operator<=>(const DenseSet& a, const DenseSet& b) = default;
  friend bool operator==(const DenseSet& a, const DenseSet& b) = default;

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
  constexpr bool empty() const { return bitset_ == Bitset{}; }

  // Returns the number of elements the set has.
  constexpr size_t size() const { return bitset_.num_set_bits(); }

  // Returns the maximum number of elements the set can have.
  constexpr size_t max_size() const { return kMaxBitIndex + 1; }

  // Modifiers.

  // Clears the contents.
  constexpr void clear() { bitset_ = {}; }

  // Inserts value |x| if it is not present yet, and returns an iterator to the
  // inserted or existing element and a boolean that indicates whether the
  // insertion took place.
  constexpr std::pair<iterator, bool> insert(T x) {
    bool contained = contains(x);
    bitset_.set_bit(value_to_index(x));
    return {find(x), !contained};
  }

  // Inserts all values of |xs| into the present set.
  constexpr void insert_all(const DenseSet& xs) { bitset_ |= xs.bitset_; }

  // Erases all elements that are not present in both `*this` and `xs`.
  constexpr void intersect(const DenseSet& xs) { bitset_ &= xs.bitset_; }

  // Erases the element whose index matches the index of |x| and returns the
  // number of erased elements (0 or 1).
  size_t erase(T x) {
    bool contained = contains(x);
    bitset_.unset_bit(value_to_index(x));
    return contained ? 1 : 0;
  }

  // Erases the element |*it| and returns an iterator to its successor.
  iterator erase(const_iterator it) {
    DCHECK(it.owner_ == this && it.dereferenceable());
    bitset_.unset_bit(it.index_);
    it.Skip(const_iterator::kForward);
    return it;
  }

  // Erases the elements [first,last) and returns |last|.
  iterator erase(const_iterator first, const_iterator last) {
    DCHECK(first.owner_ == this && last.owner_ == this);
    while (first != last) {
      bitset_.unset_bit(first.index_);
      ++first;
    }
    return last;
  }

  // Erases all values of |xs| into the present set.
  void erase_all(const DenseSet& xs) { bitset_ &= ~xs.bitset_; }

  // Lookup.

  // Returns 1 if |x| is an element, otherwise 0.
  constexpr size_t count(T x) const { return contains(x) ? 1 : 0; }

  // Returns an iterator to the element |x| if it exists, otherwise end().
  constexpr const_iterator find(T x) const {
    return contains(x) ? const_iterator(this, value_to_index(x)) : cend();
  }

  // Returns true if |x| is an element, else |false|.
  constexpr bool contains(T x) const {
    return bitset_.get_bit(value_to_index(x));
  }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_none(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) == Bitset{};
  }

  // Returns true if some element of |xs| is an element, else |false|.
  bool contains_any(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) != Bitset{};
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

  using Bitset = internal::Bitset<Word, kNumWords>;

  static constexpr Index value_to_index(T x) {
    DCHECK_LE(Traits::kMinValue, x);
    DCHECK_LE(x, Traits::kMaxValue);
    return base::checked_cast<Index>(to_underlying(x) -
                                     to_underlying(Traits::kMinValue));
  }

  static constexpr T index_to_value(Index i) {
    DCHECK_LE(i, kMaxBitIndex);
    return static_cast<T>(base::checked_cast<UnderlyingType>(i) +
                          to_underlying(Traits::kMinValue));
  }

  Bitset bitset_{};
};

template <typename T, typename... Ts>
  requires(std::same_as<T, Ts> && ...)
DenseSet(T, Ts...) -> DenseSet<T>;

template <typename InputIt, typename Proj>
DenseSet(InputIt, InputIt, Proj) -> DenseSet<std::remove_cvref_t<
    std::invoke_result_t<Proj, std::iter_value_t<InputIt>>>>;

template <typename Range, typename Proj>
DenseSet(Range, Proj) -> DenseSet<std::remove_cvref_t<
    std::invoke_result_t<Proj, std::ranges::range_value_t<Range>>>>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
