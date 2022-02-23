// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_ORDINAL_H_
#define COMPONENTS_SYNC_BASE_ORDINAL_H_

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/check_op.h"
#include "base/json/string_escape.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace syncer {

namespace mojom {
class StringOrdinalDataView;
}

// An Ordinal<T> is an object that can be used for ordering. The
// Ordinal<T> class has an unbounded dense strict total order, which
// mean for any Ordinal<T>s a, b and c:
//
//  - a < b and b < c implies a < c (transitivity);
//  - exactly one of a < b, b < a and a = b holds (trichotomy);
//  - if a < b, there is a Ordinal<T> x such that a < x < b (density);
//  - there are Ordinals<T> x and y such that x < a < y (unboundedness).
//
// This means that when Ordinal<T> is used for sorting a list, if any
// item changes its position in the list, only its Ordinal<T> value
// has to change to represent the new order, and all the other values
// can stay the same.
//
// An Ordinal<T> is internally represented as an array of bytes, so it
// can be serialized to and deserialized from disk.
//
// The Traits class should look like the following:
//
//   // Don't forget to #include <stdint.h> and <stddef.h>.
//   struct MyOrdinalTraits {
//     // There must be at least two distinct values greater than kZeroDigit
//     // and less than kMaxDigit.
//     static const uint8_t kZeroDigit = '0';
//     static const uint8_t kMaxDigit = '9';
//     // kMinLength must be positive.
//     static const size_t kMinLength = 1;
//   };
//
// An Ordinal<T> is valid iff its corresponding string has at least
// kMinLength characters, does not contain any characters less than
// kZeroDigit or greater than kMaxDigit, is not all zero digits, and
// does not have any unnecessary trailing zero digits.
//
// Note that even if the native char type is signed, strings still
// compare as if their they are unsigned.  (This is explicitly in
// C++11 but not in C++98, even though all implementations do so
// anyway in practice.)  Thus, it is safe to use any byte range for
// Ordinal<T>s.
template <typename Traits>
class Ordinal {
 public:
  // Functors for use with STL algorithms and containers.
  class LessThanFn {
   public:
    LessThanFn();

    bool operator()(const Ordinal<Traits>& lhs,
                    const Ordinal<Traits>& rhs) const;
  };

  class EqualsFn {
   public:
    EqualsFn();

    bool operator()(const Ordinal<Traits>& lhs,
                    const Ordinal<Traits>& rhs) const;
  };

  // Creates an Ordinal from the given string of bytes. The Ordinal
  // may be valid or invalid.
  explicit Ordinal(const std::string& bytes);

  // Creates an invalid Ordinal.
  Ordinal();

  // Creates a valid initial Ordinal. This is called to create the first
  // element of Ordinal list (i.e. before we have any other values we can
  // generate from).
  static Ordinal CreateInitialOrdinal();

  // Returns true iff this Ordinal is valid.  This takes constant
  // time.
  bool IsValid() const;

  // Returns true iff |*this| == |other| or |*this| and |other|
  // are both invalid.
  bool EqualsOrBothInvalid(const Ordinal& other) const;

  // Returns a printable string representation of the Ordinal suitable
  // for logging.
  std::string ToDebugString() const;

  // All remaining functions can only be called if IsValid() holds.
  // It is an error to call them if IsValid() is false.

  // Order-related functions.

  // Returns true iff |*this| < |other|.
  bool LessThan(const Ordinal& other) const;

  // Returns true iff |*this| > |other|.
  bool GreaterThan(const Ordinal& other) const;

  // Returns true iff |*this| == |other| (i.e. |*this| < |other| and
  // |other| < |*this| are both false).
  bool Equals(const Ordinal& other) const;

  // Given |*this| != |other|, returns a Ordinal x such that
  // min(|*this|, |other|) < x < max(|*this|, |other|). It is an error
  // to call this function when |*this| == |other|.
  Ordinal CreateBetween(const Ordinal& other) const;

  // Returns a Ordinal |x| such that |x| < |*this|.
  Ordinal CreateBefore() const;

  // Returns a Ordinal |x| such that |*this| < |x|.
  Ordinal CreateAfter() const;

  // Returns the string of bytes representing the Ordinal.  It is
  // guaranteed that an Ordinal constructed from the returned string
  // will be valid.
  std::string ToInternalValue() const;

  // Use of copy constructor and default assignment for this class is allowed.

  // Constants for Ordinal digits.
  static const uint8_t kZeroDigit = Traits::kZeroDigit;
  static const uint8_t kMaxDigit = Traits::kMaxDigit;
  static const size_t kMinLength = Traits::kMinLength;
  static const uint8_t kOneDigit = kZeroDigit + 1;
  static const uint8_t kMidDigit = kOneDigit + (kMaxDigit - kOneDigit) / 2;
  static const unsigned int kMidDigitValue = kMidDigit - kZeroDigit;
  static const unsigned int kMaxDigitValue = kMaxDigit - kZeroDigit;
  static const unsigned int kRadix = kMaxDigitValue + 1;

  static_assert(kOneDigit > kZeroDigit, "incorrect ordinal one digit");
  static_assert(kMidDigit > kOneDigit, "incorrect ordinal mid digit");
  static_assert(kMaxDigit > kMidDigit, "incorrect ordinal max digit");
  static_assert(kMinLength > 0, "incorrect ordinal min length");
  static_assert(kMidDigitValue > 1, "incorrect ordinal mid digit");
  static_assert(kMaxDigitValue > kMidDigitValue, "incorrect ordinal max digit");
  static_assert(kRadix == kMaxDigitValue + 1, "incorrect ordinal radix");

 private:
  friend struct mojo::StructTraits<syncer::mojom::StringOrdinalDataView,
                                   Ordinal<Traits>>;

  // Returns true iff the given byte string satisfies the criteria for
  // a valid Ordinal.
  static bool IsValidOrdinalBytes(const std::string& bytes);

  // Returns the length that bytes.substr(0, length) would be with
  // trailing zero digits removed.
  static size_t GetLengthWithoutTrailingZeroDigits(const std::string& bytes,
                                                   size_t length);

  // Returns the digit at position i, padding with zero digits if
  // required.
  static uint8_t GetDigit(const std::string& bytes, size_t i);

  // Returns the digit value at position i, padding with 0 if required.
  static int GetDigitValue(const std::string& bytes, size_t i);

  // Adds the given value to |bytes| at position i, carrying when
  // necessary.  Returns the left-most carry.
  static int AddDigitValue(std::string* bytes, size_t i, int digit_value);

  // Returns the proper length |bytes| should be resized to, i.e. the
  // smallest length such that |bytes| is still greater than
  // |lower_bound| and is still valid.  |bytes| should be greater than
  // |lower_bound|.
  static size_t GetProperLength(const std::string& lower_bound,
                                const std::string& bytes);

  // Compute the midpoint ordinal byte string that is between |start|
  // and |end|.
  static std::string ComputeMidpoint(const std::string& start,
                                     const std::string& end);

  // Create a Ordinal that is lexigraphically greater than |start| and
  // lexigraphically less than |end|. The returned Ordinal will be roughly
  // between |start| and |end|.
  static Ordinal<Traits> CreateOrdinalBetween(const Ordinal<Traits>& start,
                                              const Ordinal<Traits>& end);

  // The internal byte string representation of the Ordinal.  Never
  // changes after construction except for assignment.
  std::string bytes_;

  // A cache of the result of IsValidOrdinalBytes(bytes_).
  bool is_valid_;
};

template <typename Traits>
const uint8_t Ordinal<Traits>::kZeroDigit;
template <typename Traits>
const uint8_t Ordinal<Traits>::kMaxDigit;
template <typename Traits>
const size_t Ordinal<Traits>::kMinLength;
template <typename Traits>
const uint8_t Ordinal<Traits>::kOneDigit;
template <typename Traits>
const uint8_t Ordinal<Traits>::kMidDigit;
template <typename Traits>
const unsigned int Ordinal<Traits>::kMidDigitValue;
template <typename Traits>
const unsigned int Ordinal<Traits>::kMaxDigitValue;
template <typename Traits>
const unsigned int Ordinal<Traits>::kRadix;

template <typename Traits>
Ordinal<Traits>::LessThanFn::LessThanFn() = default;

template <typename Traits>
bool Ordinal<Traits>::LessThanFn::operator()(const Ordinal<Traits>& lhs,
                                             const Ordinal<Traits>& rhs) const {
  return lhs.LessThan(rhs);
}

template <typename Traits>
Ordinal<Traits>::EqualsFn::EqualsFn() = default;

template <typename Traits>
bool Ordinal<Traits>::EqualsFn::operator()(const Ordinal<Traits>& lhs,
                                           const Ordinal<Traits>& rhs) const {
  return lhs.Equals(rhs);
}

template <typename Traits>
bool operator==(const Ordinal<Traits>& lhs, const Ordinal<Traits>& rhs) {
  return lhs.EqualsOrBothInvalid(rhs);
}

template <typename Traits>
bool operator!=(const Ordinal<Traits>& lhs, const Ordinal<Traits>& rhs) {
  return !(lhs == rhs);
}

template <typename Traits>
Ordinal<Traits>::Ordinal(const std::string& bytes)
    : bytes_(bytes), is_valid_(IsValidOrdinalBytes(bytes_)) {}

template <typename Traits>
Ordinal<Traits>::Ordinal() : is_valid_(false) {}

template <typename Traits>
Ordinal<Traits> Ordinal<Traits>::CreateInitialOrdinal() {
  std::string bytes(Traits::kMinLength, kZeroDigit);
  bytes[0] = kMidDigit;
  return Ordinal(bytes);
}

template <typename Traits>
bool Ordinal<Traits>::IsValid() const {
  DCHECK_EQ(IsValidOrdinalBytes(bytes_), is_valid_);
  return is_valid_;
}

template <typename Traits>
bool Ordinal<Traits>::EqualsOrBothInvalid(const Ordinal& other) const {
  if (!IsValid() && !other.IsValid())
    return true;

  if (!IsValid() || !other.IsValid())
    return false;

  return Equals(other);
}

template <typename Traits>
std::string Ordinal<Traits>::ToDebugString() const {
  std::string debug_string =
      base::EscapeBytesAsInvalidJSONString(bytes_, false /* put_in_quotes */);
  if (!is_valid_) {
    debug_string = "INVALID[" + debug_string + "]";
  }
  return debug_string;
}

template <typename Traits>
bool Ordinal<Traits>::LessThan(const Ordinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ < other.bytes_;
}

template <typename Traits>
bool Ordinal<Traits>::GreaterThan(const Ordinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ > other.bytes_;
}

template <typename Traits>
bool Ordinal<Traits>::Equals(const Ordinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ == other.bytes_;
}

template <typename Traits>
Ordinal<Traits> Ordinal<Traits>::CreateBetween(const Ordinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  CHECK(!Equals(other));

  if (LessThan(other)) {
    return CreateOrdinalBetween(*this, other);
  } else {
    return CreateOrdinalBetween(other, *this);
  }
}

template <typename Traits>
Ordinal<Traits> Ordinal<Traits>::CreateBefore() const {
  CHECK(IsValid());
  // Create the smallest valid Ordinal of the appropriate length
  // to be the minimum boundary.
  const size_t length = bytes_.length();
  std::string start(length, kZeroDigit);
  start[length - 1] = kOneDigit;
  if (start == bytes_) {
    start[length - 1] = kZeroDigit;
    start += kOneDigit;
  }

  // Even though |start| is already a valid Ordinal that is less
  // than |*this|, we don't return it because we wouldn't have much space in
  // front of it to insert potential future values.
  return CreateBetween(Ordinal(start));
}

template <typename Traits>
Ordinal<Traits> Ordinal<Traits>::CreateAfter() const {
  CHECK(IsValid());
  // Create the largest valid Ordinal of the appropriate length to be
  // the maximum boundary.
  std::string end(bytes_.length(), kMaxDigit);
  if (end == bytes_)
    end += kMaxDigit;

  // Even though |end| is already a valid Ordinal that is greater than
  // |*this|, we don't return it because we wouldn't have much space after
  // it to insert potential future values.
  return CreateBetween(Ordinal(end));
}

template <typename Traits>
std::string Ordinal<Traits>::ToInternalValue() const {
  CHECK(IsValid());
  return bytes_;
}

template <typename Traits>
bool Ordinal<Traits>::IsValidOrdinalBytes(const std::string& bytes) {
  const size_t length = bytes.length();
  if (length < kMinLength)
    return false;

  bool found_non_zero = false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = bytes[i];
    if (byte < kZeroDigit || byte > kMaxDigit)
      return false;
    if (byte > kZeroDigit)
      found_non_zero = true;
  }
  if (!found_non_zero)
    return false;

  if (length > kMinLength) {
    const uint8_t last_byte = bytes[length - 1];
    if (last_byte == kZeroDigit)
      return false;
  }

  return true;
}

template <typename Traits>
size_t Ordinal<Traits>::GetLengthWithoutTrailingZeroDigits(
    const std::string& bytes,
    size_t length) {
  DCHECK(!bytes.empty());
  DCHECK_GT(length, 0U);

  size_t end_position =
      bytes.find_last_not_of(static_cast<char>(kZeroDigit), length - 1);

  // If no non kZeroDigit is found then the string is a string of all zeros
  // digits so we return 0 as the correct length.
  if (end_position == std::string::npos)
    return 0;

  return end_position + 1;
}

template <typename Traits>
uint8_t Ordinal<Traits>::GetDigit(const std::string& bytes, size_t i) {
  return (i < bytes.length()) ? bytes[i] : kZeroDigit;
}

template <typename Traits>
int Ordinal<Traits>::GetDigitValue(const std::string& bytes, size_t i) {
  return GetDigit(bytes, i) - kZeroDigit;
}

template <typename Traits>
int Ordinal<Traits>::AddDigitValue(std::string* bytes,
                                   size_t i,
                                   int digit_value) {
  DCHECK_LT(i, bytes->length());

  for (int j = static_cast<int>(i); j >= 0 && digit_value > 0; --j) {
    int byte_j_value = GetDigitValue(*bytes, j) + digit_value;
    digit_value = byte_j_value / kRadix;
    DCHECK_LE(digit_value, 1);
    byte_j_value %= kRadix;
    (*bytes)[j] = static_cast<char>(kZeroDigit + byte_j_value);
  }
  return digit_value;
}

template <typename Traits>
size_t Ordinal<Traits>::GetProperLength(const std::string& lower_bound,
                                        const std::string& bytes) {
  CHECK_GT(bytes, lower_bound);

  size_t drop_length =
      GetLengthWithoutTrailingZeroDigits(bytes, bytes.length());
  // See if the |ordinal| can be truncated after its last non-zero
  // digit without affecting the ordering.
  if (drop_length > kMinLength) {
    size_t truncated_length =
        GetLengthWithoutTrailingZeroDigits(bytes, drop_length - 1);

    if (truncated_length > 0 &&
        bytes.compare(0, truncated_length, lower_bound) > 0)
      drop_length = truncated_length;
  }
  return std::max(drop_length, kMinLength);
}

template <typename Traits>
std::string Ordinal<Traits>::ComputeMidpoint(const std::string& start,
                                             const std::string& end) {
  size_t max_size = std::max(start.length(), end.length()) + 1;
  std::string midpoint(max_size, kZeroDigit);

  // Perform the operation (start + end) / 2 left-to-right by
  // maintaining a "forward carry" which is either 0 or
  // kMidDigitValue.  AddDigitValue() is in general O(n), but this
  // operation is still O(n) despite that; calls to AddDigitValue()
  // will overflow at most to the last position where AddDigitValue()
  // last overflowed.
  int forward_carry = 0;
  for (size_t i = 0; i < max_size; ++i) {
    const int sum_value = GetDigitValue(start, i) + GetDigitValue(end, i);
    const int digit_value = sum_value / 2 + forward_carry;
    // AddDigitValue returning a non-zero carry would imply that
    // midpoint[0] >= kMaxDigit, which one can show is impossible.
    CHECK_EQ(AddDigitValue(&midpoint, i, digit_value), 0);
    forward_carry = (sum_value % 2 == 1) ? kMidDigitValue : 0;
  }
  DCHECK_EQ(forward_carry, 0);

  return midpoint;
}

template <typename Traits>
Ordinal<Traits> Ordinal<Traits>::CreateOrdinalBetween(
    const Ordinal<Traits>& start,
    const Ordinal<Traits>& end) {
  CHECK(start.IsValid());
  CHECK(end.IsValid());
  CHECK(start.LessThan(end));
  const std::string& start_bytes = start.ToInternalValue();
  const std::string& end_bytes = end.ToInternalValue();
  DCHECK_LT(start_bytes, end_bytes);

  std::string midpoint = ComputeMidpoint(start_bytes, end_bytes);
  const size_t proper_length = GetProperLength(start_bytes, midpoint);
  midpoint.resize(proper_length, kZeroDigit);

  DCHECK_GT(midpoint, start_bytes);
  DCHECK_LT(midpoint, end_bytes);

  Ordinal<Traits> midpoint_ordinal(midpoint);
  DCHECK(midpoint_ordinal.IsValid());
  return midpoint_ordinal;
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_ORDINAL_H_
