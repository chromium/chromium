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
#include "base/types/cxx23_to_underlying.h"

namespace autofill {

namespace internal {

// The number of bits in `T`.
template <typename T>
static constexpr size_t kBitsPer = sizeof(T) * CHAR_BIT;

// Returns the index of the next 1 that is at or to the right of `index`.
// If there is none, returns -1.
//
// For example, PreviousBitIndex<uint8_t>(0b11010101), 5) returns 4:
//   Word:   0b11010001
//               ^^
//   Index:    76543210
template <typename Word>
constexpr int PreviousBitIndex(Word word, size_t index) {
  DCHECK_LT(index, kBitsPer<Word>);
  // Shifting by `kBitsPer<Word>` is undefined behavior, so we must not shift by
  // `index + 1`.
  const Word mask =
      (static_cast<Word>(1) << index) | ((static_cast<Word>(1) << index) - 1);
  return base::checked_cast<int>(kBitsPer<Word>) - 1 -
         std::countl_zero(static_cast<Word>(word & mask));
}

// Returns the index of the next 1 that is at or to the left of `index`.
// If there is none, returns `kBitsPer<Word>`.
//
// For example, NextBitIndex<uint8_t>(0b100001011), 2) returns 3:
//   Word:   0b10001011
//                 ^^
//   Index:    76543210
template <typename Word>
constexpr int NextBitIndex(Word word, size_t index) {
  DCHECK_LT(index, kBitsPer<Word>);
  const Word mask = ~((static_cast<Word>(1) << index) - 1);
  return std::countr_zero(static_cast<Word>(word & mask));
}

// A bitset represented as `std::array<Word, kNumWords>.
// There's a specialization further down for `kNumWords == 1`.
template <typename Word, size_t kNumWords>
class Bitset final {
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
    DCHECK_LT(index, words_.size() * kBitsPer<Word>);
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    return words_[word] & (static_cast<Word>(1) << bit);
  }

  constexpr void set_bit(size_t index) {
    DCHECK_LT(index, words_.size() * kBitsPer<Word>);
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    words_[word] |= static_cast<Word>(1) << bit;
  }

  constexpr void unset_bit(size_t index) {
    DCHECK_LT(index, words_.size() * kBitsPer<Word>);
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    words_[word] &= ~(static_cast<Word>(1) << bit);
  }

  // Returns the maximum value that is `<= index` and points to a set bit, or
  // -1 if none exists.
  constexpr int previous_set_bit(size_t index) const {
    DCHECK_LT(index, words_.size() * kBitsPer<Word>);
    index = std::min(index, words_.size() * kBitsPer<Word> - 1);
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    do {
      const int previous_bit = PreviousBitIndex(words_[word], bit);
      DCHECK_GE(previous_bit, -1);
      DCHECK_LT(previous_bit, base::checked_cast<int>(kBitsPer<Word>));
      if (previous_bit >= 0) {
        return word * kBitsPer<Word> + previous_bit;
      }
      bit = kBitsPer<Word> - 1;
    } while (word-- > 0);
    return -1;
  }

  // Returns the minimum value that is `>= index` and points to a set bit, or
  // `words_.size() * kBitsPer<Word>` if none exists.
  constexpr size_t next_set_bit(size_t index) const {
    DCHECK_LT(index, words_.size() * kBitsPer<Word>);
    size_t word = index / kBitsPer<Word>;
    size_t bit = index % kBitsPer<Word>;
    while (word < words_.size()) {
      const int next_bit = NextBitIndex(words_[word], bit);
      DCHECK_GE(next_bit, 0);
      DCHECK_LE(next_bit, base::checked_cast<int>(kBitsPer<Word>));
      if (next_bit < base::checked_cast<int>(kBitsPer<Word>)) {
        return word * kBitsPer<Word> + next_bit;
      }
      ++word;
      bit = 0;
    }
    return words_.size() * kBitsPer<Word>;
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
class Bitset<Word, 1u> final {
 public:
  constexpr Bitset() = default;

  constexpr size_t num_set_bits() const { return std::popcount(word_); }

  constexpr int previous_set_bit(size_t index) const {
    DCHECK_LT(index, kBitsPer<Word>);
    return PreviousBitIndex(word_, index);
  }

  constexpr size_t next_set_bit(size_t index) const {
    DCHECK_LT(index, kBitsPer<Word>);
    return NextBitIndex(word_, index);
  }

  constexpr bool get_bit(size_t index) const {
    DCHECK_LT(index, kBitsPer<Word>);
    return word_ & (static_cast<Word>(1) << index);
  }

  constexpr void set_bit(size_t index) {
    DCHECK_LT(index, kBitsPer<Word>);
    word_ |= static_cast<Word>(1) << index;
  }

  constexpr void unset_bit(size_t index) {
    DCHECK_LT(index, kBitsPer<Word>);
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
  Word word_{};
};

template <typename T, typename Traits>
concept ValidDenseSetTraits =
    std::integral<typename Traits::UnderlyingType> &&
    std::same_as<decltype(Traits::from_underlying(
                     std::declval<typename Traits::UnderlyingType>())),
                 T> &&
    std::same_as<decltype(Traits::to_underlying(std::declval<T>())),
                 typename Traits::UnderlyingType> &&
    std::same_as<decltype(Traits::is_valid(std::declval<T>())), bool> &&
    std::same_as<decltype(Traits::kMinValue), const T> &&
    std::same_as<decltype(Traits::kMaxValue), const T> &&
    std::same_as<decltype(Traits::kPacked), const bool>;

}  // namespace internal

// Helper for traits for integer DenseSets.
template <typename T, T kMinValueT, T kMaxValueT>
  requires(std::is_integral_v<T>)
struct IntegralDenseSetTraits {
  using UnderlyingType = T;

  static constexpr T from_underlying(UnderlyingType x) { return x; }
  static constexpr UnderlyingType to_underlying(T x) { return x; }
  static constexpr bool is_valid(T x) { return true; }

  static constexpr T kMinValue = kMinValueT;
  static constexpr T kMaxValue = kMaxValueT;
  static constexpr bool kPacked = false;
};

// Helper for traits for enum DenseSets.
template <typename T, T kMinValueT, T kMaxValueT>
  requires(std::is_enum_v<T>)
struct EnumDenseSetTraits {
  using UnderlyingType = std::underlying_type_t<T>;

  static constexpr T from_underlying(UnderlyingType x) {
    return static_cast<T>(x);
  }
  static constexpr UnderlyingType to_underlying(T x) {
    return base::to_underlying(x);
  }
  static constexpr bool is_valid(T x) { return true; }

  static constexpr T kMinValue = kMinValueT;
  static constexpr T kMaxValue = kMaxValueT;
  static constexpr bool kPacked = false;
};

// The default traits.
template <typename T, typename = void>
struct DenseSetTraits {};

template <typename T>
  requires(std::is_enum_v<T>)
struct DenseSetTraits<T> : public EnumDenseSetTraits<T, T(0), T::kMaxValue> {};

// A set container with a std::set<T>-like interface for a type T that has a
// dense and small integral representation. DenseSet is particularly suited for
// enums.
//
// The order of the elements in the container corresponds to their integer
// representation.
//
// Traits::UnderlyingType is the integral representation of the stored types.
// Traits::to_underlying() and Traits::from_underlying() convert between T and
// Traits::UnderlyingType.
//
// The lower and upper bounds of elements storable in a container are
// [Traits::kMinValue, Traits::kMaxValue].
// For enums, the default is [T(0), T::kMaxValue].
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
  requires(internal::ValidDenseSetTraits<T, Traits>)
class DenseSet {
 private:
  // For arithmetic on `T`.
  using UnderlyingType = typename Traits::UnderlyingType;

  // The index of a bit in the underlying bitset. Use
  // value_to_index() and index_to_value() for conversion.
  using Index = std::make_unsigned_t<UnderlyingType>;

  static constexpr UnderlyingType to_underlying(T x) {
    return Traits::to_underlying(x);
  }

  static constexpr T from_underlying(UnderlyingType x) {
    return Traits::from_underlying(x);
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

    friend constexpr bool operator==(const Iterator& a, const Iterator& b) {
      DCHECK(a.owner_);
      DCHECK_EQ(a.owner_, b.owner_);
      return a.index_ == b.index_;
    }

    constexpr T operator*() const {
      DCHECK_LT(index_, owner_->max_size());
      DCHECK(owner_->bitset_.get_bit(index_));
      return index_to_value(index_);
    }

    constexpr Iterator& operator++() {
      ++index_;
      SkipForward();
      return *this;
    }

    constexpr Iterator operator++(int) {
      auto that = *this;
      operator++();
      return that;
    }

    constexpr Iterator& operator--() {
      --index_;
      SkipBackward();
      return *this;
    }

    constexpr Iterator operator--(int) {
      auto that = *this;
      operator--();
      return that;
    }

   private:
    friend DenseSet;

    constexpr Iterator(const DenseSet* owner, Index index)
        : owner_(owner), index_(index) {}

    constexpr void SkipBackward() {
      DCHECK_LE(index_, owner_->max_size());
      if (index_ < owner_->max_size()) {
        index_ = std::max(owner_->bitset_.previous_set_bit(index_), 0);
      }
    }

    constexpr void SkipForward() {
      DCHECK_LE(index_, owner_->max_size());
      if (index_ < owner_->max_size()) {
        index_ =
            std::min(owner_->bitset_.next_set_bit(index_), owner_->max_size());
      }
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

  constexpr DenseSet(const DenseSet&) = default;
  constexpr DenseSet& operator=(const DenseSet&) = default;

  constexpr ~DenseSet() = default;

  // Returns a set containing all valid values from `kMinValue` to `kMaxValue`.
  static consteval DenseSet all() {
    DenseSet set;
    for (Index x = value_to_index(Traits::kMinValue);
         x <= value_to_index(Traits::kMaxValue); ++x) {
      T value = index_to_value(x);
      if (Traits::is_valid(value)) {
        set.insert(value);
      }
    }
    return set;
  }

  // Returns a raw bitmask. Useful for serialization.
  constexpr base::span<const Word, kNumWords> data() const LIFETIME_BOUND {
    return bitset_.data();
  }

  friend auto operator<=>(const DenseSet& a, const DenseSet& b) = default;
  friend bool operator==(const DenseSet& a, const DenseSet& b) = default;

  // Iterators.

  // Returns an iterator to the beginning.
  constexpr iterator begin() const {
    const_iterator it(this, 0);
    it.SkipForward();
    return it;
  }
  constexpr const_iterator cbegin() const { return begin(); }

  // Returns an iterator to the end.
  constexpr iterator end() const { return iterator(this, max_size()); }
  constexpr const_iterator cend() const { return end(); }

  // Returns a reverse iterator to the beginning.
  constexpr reverse_iterator rbegin() const { return reverse_iterator(end()); }
  constexpr const_reverse_iterator crbegin() const { return rbegin(); }

  // Returns a reverse iterator to the end.
  constexpr reverse_iterator rend() const { return reverse_iterator(begin()); }
  constexpr const_reverse_iterator crend() const { return rend(); }

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
  constexpr size_t erase(T x) {
    bool contained = contains(x);
    bitset_.unset_bit(value_to_index(x));
    return contained ? 1 : 0;
  }

  // Erases the element |*it| and returns an iterator to its successor.
  iterator erase(const_iterator it) {
    DCHECK(it.owner_ == this);
    DCHECK(bitset_.get_bit(it.index_));
    bitset_.unset_bit(it.index_);
    it.SkipForward();
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
  constexpr void erase_all(const DenseSet& xs) { bitset_ &= ~xs.bitset_; }

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

  // Returns true if no element of |xs| is an element, else |false|.
  constexpr bool contains_none(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) == Bitset{};
  }

  // Returns true if some element of |xs| is an element, else |false|.
  constexpr bool contains_any(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) != Bitset{};
  }

  // Returns true if every elements of |xs| is an element, else |false|.
  constexpr bool contains_all(const DenseSet& xs) const {
    return (bitset_ & xs.bitset_) == xs.bitset_;
  }

  // Returns an iterator to the first element not less than the |x|, or end().
  const_iterator lower_bound(T x) const {
    const_iterator it(this, value_to_index(x));
    it.SkipForward();
    return it;
  }

  // Returns an iterator to the first element greater than |x|, or end().
  const_iterator upper_bound(T x) const {
    const_iterator it(this, value_to_index(x) + 1);
    it.SkipForward();
    return it;
  }

 private:
  friend Iterator;

  using Bitset = internal::Bitset<Word, kNumWords>;

  static constexpr Index value_to_index(T x) {
    DCHECK_LE(to_underlying(Traits::kMinValue), to_underlying(x));
    DCHECK_LE(to_underlying(x), to_underlying(Traits::kMaxValue));
    return base::checked_cast<Index>(to_underlying(x) -
                                     to_underlying(Traits::kMinValue));
  }

  static constexpr T index_to_value(Index i) {
    DCHECK_LE(i, kMaxBitIndex);
    return from_underlying(base::checked_cast<UnderlyingType>(i) +
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

template <typename T, typename Traits, typename... Ts>
  requires((std::same_as<Ts, DenseSet<T, Traits>>) && ...)
[[nodiscard]] constexpr DenseSet<T, Traits> Intersection(DenseSet<T, Traits> s,
                                                         const Ts... ts) {
  (s.intersect(ts), ...);
  return s;
}

template <typename T, typename Traits, typename... Ts>
  requires((std::same_as<Ts, DenseSet<T, Traits>>) && ...)
[[nodiscard]] constexpr DenseSet<T, Traits> Union(DenseSet<T, Traits> s,
                                                  const Ts... ts) {
  (s.insert_all(ts), ...);
  return s;
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_DENSE_SET_H_
