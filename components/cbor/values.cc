// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/cbor/values.h"

#include <new>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/constants.h"

namespace cbor {

// static
Value Value::InvalidUTF8StringValueForTesting(std::string_view in_string) {
  return Value(
      base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(in_string.data()), in_string.size()),
      Type::INVALID_UTF8);
}

Value::Value() noexcept : type_(Type::NONE) {}

Value::Value(Value&& that) noexcept {
  InternalMoveConstructFrom(std::move(that));
}

Value::Value(Type type) : type_(type) {
  // Initialize with the default value.
  switch (type_) {
    case Type::UNSIGNED:
    case Type::NEGATIVE:
      integer_value_ = 0;
      return;
    case Type::INVALID_UTF8:
    case Type::BYTE_STRING:
      new (&bytestring_value_) BinaryValue();
      return;
    case Type::STRING:
      new (&string_value_) std::string();
      return;
    case Type::ARRAY:
      new (&array_value_) ArrayValue();
      return;
    case Type::MAP:
      new (&map_value_) MapValue();
      return;
    case Type::TAG:
      NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
      return;
    case Type::SIMPLE_VALUE:
      simple_value_ = Value::SimpleValue::UNDEFINED;
      return;
    case Type::FLOAT_VALUE:
      float_value_ = 0.0;
      return;
    case Type::NONE:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

Value::Value(SimpleValue in_simple)
    : type_(Type::SIMPLE_VALUE), simple_value_(in_simple) {
  CHECK(static_cast<int>(in_simple) >= 20 && static_cast<int>(in_simple) <= 23);
}

Value::Value(bool boolean_value) : type_(Type::SIMPLE_VALUE) {
  simple_value_ = boolean_value ? Value::SimpleValue::TRUE_VALUE
                                : Value::SimpleValue::FALSE_VALUE;
}

Value::Value(double float_value)
    : type_(Type::FLOAT_VALUE), float_value_(float_value) {}

Value::Value(int integer_value)
    : Value(base::checked_cast<int64_t>(integer_value)) {}

Value::Value(int64_t integer_value) : integer_value_(integer_value) {
  type_ = integer_value >= 0 ? Type::UNSIGNED : Type::NEGATIVE;
}

Value::Value(base::span<const uint8_t> in_bytes)
    : type_(Type::BYTE_STRING),
      bytestring_value_(in_bytes.begin(), in_bytes.end()) {}

Value::Value(base::span<const uint8_t> in_bytes, Type type)
    : type_(type), bytestring_value_(in_bytes.begin(), in_bytes.end()) {
  DCHECK(type_ == Type::BYTE_STRING || type_ == Type::INVALID_UTF8);
}

Value::Value(BinaryValue&& in_bytes) noexcept
    : type_(Type::BYTE_STRING), bytestring_value_(std::move(in_bytes)) {}

Value::Value(const char* in_string, Type type)
    : Value(std::string_view(in_string), type) {}

Value::Value(std::string&& in_string, Type type) noexcept : type_(type) {
  switch (type_) {
    case Type::STRING:
      new (&string_value_) std::string();
      string_value_ = std::move(in_string);
      DCHECK(base::IsStringUTF8(string_value_));
      break;
    case Type::BYTE_STRING:
      new (&bytestring_value_) BinaryValue();
      bytestring_value_ = BinaryValue(in_string.begin(), in_string.end());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

Value::Value(std::string_view in_string, Type type) : type_(type) {
  switch (type_) {
    case Type::STRING:
      new (&string_value_) std::string();
      string_value_ = std::string(in_string);
      DCHECK(base::IsStringUTF8(string_value_));
      break;
    case Type::BYTE_STRING:
      new (&bytestring_value_) BinaryValue();
      bytestring_value_ = BinaryValue(in_string.begin(), in_string.end());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

Value::Value(const ArrayValue& in_array) : type_(Type::ARRAY), array_value_() {
  array_value_.reserve(in_array.size());
  for (const auto& val : in_array)
    array_value_.emplace_back(val.Clone());
}

Value::Value(ArrayValue&& in_array) noexcept
    : type_(Type::ARRAY), array_value_(std::move(in_array)) {}

Value::Value(const MapValue& in_map) : type_(Type::MAP), map_value_() {
  map_value_.reserve(in_map.size());
  for (const auto& it : in_map)
    map_value_.emplace_hint(map_value_.end(), it.first.Clone(),
                            it.second.Clone());
}

Value::Value(MapValue&& in_map) noexcept
    : type_(Type::MAP), map_value_(std::move(in_map)) {}

Value& Value::operator=(Value&& that) noexcept {
  InternalCleanup();
  InternalMoveConstructFrom(std::move(that));

  return *this;
}

Value::~Value() {
  InternalCleanup();
}

Value Value::Clone() const {
  switch (type_) {
    case Type::NONE:
      return Value();
    case Type::INVALID_UTF8:
      return Value(bytestring_value_, Type::INVALID_UTF8);
    case Type::UNSIGNED:
    case Type::NEGATIVE:
      return Value(integer_value_);
    case Type::BYTE_STRING:
      return Value(bytestring_value_);
    case Type::STRING:
      return Value(string_value_);
    case Type::ARRAY:
      return Value(array_value_);
    case Type::MAP:
      return Value(map_value_);
    case Type::TAG:
      NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
      return Value();
    case Type::SIMPLE_VALUE:
      return Value(simple_value_);
    case Type::FLOAT_VALUE:
      return Value(float_value_);
  }

  NOTREACHED_IN_MIGRATION();
  return Value();
}

Value::SimpleValue Value::GetSimpleValue() const {
  CHECK(is_simple());
  return simple_value_;
}

bool Value::GetBool() const {
  CHECK(is_bool());
  return simple_value_ == SimpleValue::TRUE_VALUE;
}

double Value::GetDouble() const {
  CHECK(is_double());
  return float_value_;
}

const int64_t& Value::GetInteger() const {
  CHECK(is_integer());
  return integer_value_;
}

const int64_t& Value::GetUnsigned() const {
  CHECK(is_unsigned());
  CHECK_GE(integer_value_, 0);
  return integer_value_;
}

const int64_t& Value::GetNegative() const {
  CHECK(is_negative());
  CHECK_LT(integer_value_, 0);
  return integer_value_;
}

const std::string& Value::GetString() const {
  CHECK(is_string());
  return string_value_;
}

const Value::BinaryValue& Value::GetBytestring() const {
  CHECK(is_bytestring());
  return bytestring_value_;
}

std::string_view Value::GetBytestringAsString() const {
  CHECK(is_bytestring());
  const auto& bytestring_value = GetBytestring();
  return std::string_view(
      reinterpret_cast<const char*>(bytestring_value.data()),
      bytestring_value.size());
}

const Value::ArrayValue& Value::GetArray() const {
  CHECK(is_array());
  return array_value_;
}

const Value::MapValue& Value::GetMap() const {
  CHECK(is_map());
  return map_value_;
}

const Value::BinaryValue& Value::GetInvalidUTF8() const {
  CHECK(is_invalid_utf8());
  return bytestring_value_;
}

void Value::InternalMoveConstructFrom(Value&& that) {
  type_ = that.type_;

  switch (type_) {
    case Type::UNSIGNED:
    case Type::NEGATIVE:
      integer_value_ = that.integer_value_;
      return;
    case Type::INVALID_UTF8:
    case Type::BYTE_STRING:
      new (&bytestring_value_) BinaryValue(std::move(that.bytestring_value_));
      return;
    case Type::STRING:
      new (&string_value_) std::string(std::move(that.string_value_));
      return;
    case Type::ARRAY:
      new (&array_value_) ArrayValue(std::move(that.array_value_));
      return;
    case Type::MAP:
      new (&map_value_) MapValue(std::move(that.map_value_));
      return;
    case Type::TAG:
      NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
      return;
    case Type::SIMPLE_VALUE:
      simple_value_ = that.simple_value_;
      return;
    case Type::FLOAT_VALUE:
      float_value_ = that.float_value_;
      return;
    case Type::NONE:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void Value::InternalCleanup() {
  switch (type_) {
    case Type::BYTE_STRING:
    case Type::INVALID_UTF8:
      bytestring_value_.~BinaryValue();
      break;
    case Type::STRING:
      string_value_.~basic_string();
      break;
    case Type::ARRAY:
      array_value_.~ArrayValue();
      break;
    case Type::MAP:
      map_value_.~MapValue();
      break;
    case Type::TAG:
      NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
      break;
    case Type::NONE:
    case Type::UNSIGNED:
    case Type::NEGATIVE:
    case Type::SIMPLE_VALUE:
    case Type::FLOAT_VALUE:
      break;
  }
  type_ = Type::NONE;
}

}  // namespace cbor
