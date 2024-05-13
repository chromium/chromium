// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_STRING_ORDINAL_H_
#define COMPONENTS_SYNC_MODEL_STRING_ORDINAL_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace syncer {

namespace mojom {
class StringOrdinalDataView;
}

// WARNING, DO NOT USE! Use UniquePosition instead.
//
//
// A StringOrdinal is an object that can be used for ordering. The
// StringOrdinal class has an unbounded dense strict total order, which
// mean for any StringOrdinals a, b and c:
//
//  - a < b and b < c implies a < c (transitivity);
//  - exactly one of a < b, b < a and a = b holds (trichotomy);
//  - if a < b, there is a StringOrdinal x such that a < x < b (density);
//  - there are Ordinals<T> x and y such that x < a < y (unboundedness).
//
// This means that when StringOrdinal is used for sorting a list, if any
// item changes its position in the list, only its StringOrdinal value
// has to change to represent the new order, and all the other values
// can stay the same.
//
// A StringOrdinal is internally represented as an array of bytes, so it
// can be serialized to and deserialized from disk.
//
// A StringOrdinal is valid iff its corresponding string has at least
// kMinLength characters, does not contain any characters less than
// kZeroDigit or greater than kMaxDigit, is not all zero digits, and
// does not have any unnecessary trailing zero digits.
//
// Since StringOrdinals contain only printable characters, it is safe
// to store as a string in a protobuf.
class StringOrdinal {
 public:
  // Functors for use with STL algorithms and containers.
  class LessThanFn {
   public:
    LessThanFn();

    bool operator()(const StringOrdinal& lhs, const StringOrdinal& rhs) const;
  };

  class EqualsFn {
   public:
    EqualsFn();

    bool operator()(const StringOrdinal& lhs, const StringOrdinal& rhs) const;
  };

  // Creates an StringOrdinal from the given string of bytes. The StringOrdinal
  // may be valid or invalid.
  explicit StringOrdinal(std::string bytes);

  // Creates an invalid StringOrdinal.
  StringOrdinal();

  // Creates a valid initial StringOrdinal. This is called to create the first
  // element of StringOrdinal list (i.e. before we have any other values we can
  // generate from).
  static StringOrdinal CreateInitialOrdinal();

  // Returns true iff this StringOrdinal is valid.  This takes constant
  // time.
  bool IsValid() const;

  // Returns true iff |*this| == |other| or |*this| and |other|
  // are both invalid.
  bool EqualsOrBothInvalid(const StringOrdinal& other) const;

  // Returns a printable string representation of the StringOrdinal suitable
  // for logging.
  std::string ToDebugString() const;

  // All remaining functions can only be called if IsValid() holds.
  // It is an error to call them if IsValid() is false.

  // Order-related functions.

  // Returns true iff |*this| < |other|.
  bool LessThan(const StringOrdinal& other) const;

  // Returns true iff |*this| > |other|.
  bool GreaterThan(const StringOrdinal& other) const;

  // Returns true iff |*this| == |other| (i.e. |*this| < |other| and
  // |other| < |*this| are both false).
  bool Equals(const StringOrdinal& other) const;

  // Given |*this| != |other|, returns a StringOrdinal x such that
  // min(|*this|, |other|) < x < max(|*this|, |other|). It is an error
  // to call this function when |*this| == |other|.
  StringOrdinal CreateBetween(const StringOrdinal& other) const;

  // Returns a StringOrdinal |x| such that |x| < |*this|.
  StringOrdinal CreateBefore() const;

  // Returns a StringOrdinal |x| such that |*this| < |x|.
  StringOrdinal CreateAfter() const;

  // Returns the string of bytes representing the StringOrdinal.  It is
  // guaranteed that an StringOrdinal constructed from the returned string
  // will be valid.
  std::string ToInternalValue() const;

  // Use of copy constructor and default assignment for this class is allowed.

  // Constants for StringOrdinal digits.
  static const uint8_t kZeroDigit = 'a';
  static const uint8_t kMaxDigit = 'z';
  static const size_t kMinLength = 1;
  static const uint8_t kOneDigit = kZeroDigit + 1;
  static const uint8_t kMidDigit = kOneDigit + (kMaxDigit - kOneDigit) / 2;
  static const unsigned int kMidDigitValue = kMidDigit - kZeroDigit;
  static const unsigned int kMaxDigitValue = kMaxDigit - kZeroDigit;
  static const unsigned int kRadix = kMaxDigitValue + 1;

  static_assert(kOneDigit > kZeroDigit, "incorrect StringOrdinal one digit");
  static_assert(kMidDigit > kOneDigit, "incorrect StringOrdinal mid digit");
  static_assert(kMaxDigit > kMidDigit, "incorrect StringOrdinal max digit");
  static_assert(kMinLength > 0, "incorrect StringOrdinal min length");
  static_assert(kMidDigitValue > 1, "incorrect StringOrdinal mid digit");
  static_assert(kMaxDigitValue > kMidDigitValue,
                "incorrect StringOrdinal max digit");
  static_assert(kRadix == kMaxDigitValue + 1, "incorrect StringOrdinal radix");

 private:
  friend struct mojo::StructTraits<syncer::mojom::StringOrdinalDataView,
                                   StringOrdinal>;

  // Returns true iff the given byte string satisfies the criteria for
  // a valid StringOrdinal.
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

  // Compute the midpoint StringOrdinal byte string that is between |start|
  // and |end|.
  static std::string ComputeMidpoint(const std::string& start,
                                     const std::string& end);

  // Create a StringOrdinal that is lexigraphically greater than |start| and
  // lexigraphically less than |end|. The returned StringOrdinal will be roughly
  // between |start| and |end|.
  static StringOrdinal CreateOrdinalBetween(const StringOrdinal& start,
                                            const StringOrdinal& end);

  // The internal byte string representation of the StringOrdinal.  Never
  // changes after construction except for assignment.
  std::string bytes_;

  // A cache of the result of IsValidOrdinalBytes(bytes_).
  bool is_valid_;
};

bool operator==(const StringOrdinal& lhs, const StringOrdinal& rhs);

static_assert(StringOrdinal::kZeroDigit == 'a',
              "StringOrdinal has incorrect zero digit");
static_assert(StringOrdinal::kOneDigit == 'b',
              "StringOrdinal has incorrect one digit");
static_assert(StringOrdinal::kMidDigit == 'n',
              "StringOrdinal has incorrect mid digit");
static_assert(StringOrdinal::kMaxDigit == 'z',
              "StringOrdinal has incorrect max digit");
static_assert(StringOrdinal::kMidDigitValue == 13,
              "StringOrdinal has incorrect mid digit value");
static_assert(StringOrdinal::kMaxDigitValue == 25,
              "StringOrdinal has incorrect max digit value");
static_assert(StringOrdinal::kRadix == 26, "StringOrdinal has incorrect radix");

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_STRING_ORDINAL_H_
