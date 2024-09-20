// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/string_ordinal.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/string_escape.h"

namespace syncer {

const uint8_t StringOrdinal::kZeroDigit;
const uint8_t StringOrdinal::kMaxDigit;
const size_t StringOrdinal::kMinLength;
const uint8_t StringOrdinal::kOneDigit;
const uint8_t StringOrdinal::kMidDigit;
const unsigned int StringOrdinal::kMidDigitValue;
const unsigned int StringOrdinal::kMaxDigitValue;
const unsigned int StringOrdinal::kRadix;

StringOrdinal::LessThanFn::LessThanFn() = default;

bool StringOrdinal::LessThanFn::operator()(const StringOrdinal& lhs,
                                           const StringOrdinal& rhs) const {
  return lhs.LessThan(rhs);
}

StringOrdinal::EqualsFn::EqualsFn() = default;

bool StringOrdinal::EqualsFn::operator()(const StringOrdinal& lhs,
                                         const StringOrdinal& rhs) const {
  return lhs.Equals(rhs);
}

bool operator==(const StringOrdinal& lhs, const StringOrdinal& rhs) {
  return lhs.EqualsOrBothInvalid(rhs);
}

StringOrdinal::StringOrdinal(std::string bytes)
    : bytes_(std::move(bytes)), is_valid_(IsValidOrdinalBytes(bytes_)) {}

StringOrdinal::StringOrdinal() : is_valid_(false) {}

StringOrdinal StringOrdinal::CreateInitialOrdinal() {
  std::string bytes(kMinLength, kZeroDigit);
  bytes[0] = kMidDigit;
  return StringOrdinal(bytes);
}

bool StringOrdinal::IsValid() const {
  DCHECK_EQ(IsValidOrdinalBytes(bytes_), is_valid_);
  return is_valid_;
}

bool StringOrdinal::EqualsOrBothInvalid(const StringOrdinal& other) const {
  if (!IsValid() && !other.IsValid()) {
    return true;
  }

  if (!IsValid() || !other.IsValid()) {
    return false;
  }

  return Equals(other);
}

std::string StringOrdinal::ToDebugString() const {
  std::string debug_string =
      base::EscapeBytesAsInvalidJSONString(bytes_, false /* put_in_quotes */);
  if (!is_valid_) {
    debug_string = "INVALID[" + debug_string + "]";
  }
  return debug_string;
}

bool StringOrdinal::LessThan(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ < other.bytes_;
}

bool StringOrdinal::GreaterThan(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ > other.bytes_;
}

bool StringOrdinal::Equals(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  return bytes_ == other.bytes_;
}

StringOrdinal StringOrdinal::CreateBetween(const StringOrdinal& other) const {
  CHECK(IsValid());
  CHECK(other.IsValid());
  CHECK(!Equals(other));

  if (LessThan(other)) {
    return CreateOrdinalBetween(*this, other);
  } else {
    return CreateOrdinalBetween(other, *this);
  }
}

StringOrdinal StringOrdinal::CreateBefore() const {
  CHECK(IsValid());
  // Create the smallest valid StringOrdinal of the appropriate length
  // to be the minimum boundary.
  const size_t length = bytes_.length();
  std::string start(length, kZeroDigit);
  start[length - 1] = kOneDigit;
  if (start == bytes_) {
    start[length - 1] = kZeroDigit;
    start += kOneDigit;
  }

  // Even though |start| is already a valid StringOrdinal that is less
  // than |*this|, we don't return it because we wouldn't have much space in
  // front of it to insert potential future values.
  return CreateBetween(StringOrdinal(start));
}

StringOrdinal StringOrdinal::CreateAfter() const {
  CHECK(IsValid());
  // Create the largest valid StringOrdinal of the appropriate length to be
  // the maximum boundary.
  std::string end(bytes_.length(), kMaxDigit);
  if (end == bytes_) {
    end += kMaxDigit;
  }

  // Even though |end| is already a valid StringOrdinal that is greater than
  // |*this|, we don't return it because we wouldn't have much space after
  // it to insert potential future values.
  return CreateBetween(StringOrdinal(end));
}

std::string StringOrdinal::ToInternalValue() const {
  CHECK(IsValid());
  return bytes_;
}

bool StringOrdinal::IsValidOrdinalBytes(const std::string& bytes) {
  const size_t length = bytes.length();
  if (length < kMinLength) {
    return false;
  }

  bool found_non_zero = false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t byte = bytes[i];
    if (byte < kZeroDigit || byte > kMaxDigit) {
      return false;
    }
    if (byte > kZeroDigit) {
      found_non_zero = true;
    }
  }
  if (!found_non_zero) {
    return false;
  }

  if (length > kMinLength) {
    const uint8_t last_byte = bytes[length - 1];
    if (last_byte == kZeroDigit) {
      return false;
    }
  }

  return true;
}

size_t StringOrdinal::GetLengthWithoutTrailingZeroDigits(
    const std::string& bytes,
    size_t length) {
  DCHECK(!bytes.empty());
  DCHECK_GT(length, 0U);

  size_t end_position =
      bytes.find_last_not_of(static_cast<char>(kZeroDigit), length - 1);

  // If no non kZeroDigit is found then the string is a string of all zeros
  // digits so we return 0 as the correct length.
  if (end_position == std::string::npos) {
    return 0;
  }

  return end_position + 1;
}

uint8_t StringOrdinal::GetDigit(const std::string& bytes, size_t i) {
  return (i < bytes.length()) ? bytes[i] : kZeroDigit;
}

int StringOrdinal::GetDigitValue(const std::string& bytes, size_t i) {
  return GetDigit(bytes, i) - kZeroDigit;
}

int StringOrdinal::AddDigitValue(std::string* bytes,
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

size_t StringOrdinal::GetProperLength(const std::string& lower_bound,
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
        bytes.compare(0, truncated_length, lower_bound) > 0) {
      drop_length = truncated_length;
    }
  }
  return std::max(drop_length, kMinLength);
}

std::string StringOrdinal::ComputeMidpoint(const std::string& start,
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

StringOrdinal StringOrdinal::CreateOrdinalBetween(const StringOrdinal& start,
                                                  const StringOrdinal& end) {
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

  StringOrdinal midpoint_ordinal(midpoint);
  DCHECK(midpoint_ordinal.IsValid());
  return midpoint_ordinal;
}

}  // namespace syncer
