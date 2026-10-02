// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/values.h"

#include <ostream>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cbor {

// static
Value Value::InvalidUTF8StringValueForTesting(std::string_view in_string) {
  return Value(invalid_utf8, base::as_byte_span(in_string));
}

Value::Value(Value&&) noexcept = default;

Value::Value(bool boolean_value) noexcept : data_(boolean_value) {}

Value::Value(Null value) noexcept : data_(value) {}

Value::Value(Undefined value) noexcept : data_(value) {}

Value::Value(int integer_value)
    : Value(base::checked_cast<int64_t>(integer_value)) {}

Value::Value(int64_t integer_value) noexcept : data_(integer_value) {}

Value::Value(base::span<const uint8_t> in_bytes)
    : data_(std::in_place_type<BinaryValue>, std::from_range, in_bytes) {}

Value::Value(invalid_utf8_t, base::span<const uint8_t> in_bytes)
    : data_(std::in_place_type<InvalidUTF8>,
            BinaryValue(std::from_range, in_bytes)) {}

Value::Value(BinaryValue&& in_bytes) noexcept : data_(std::move(in_bytes)) {}

Value::Value(const char* in_string) : Value(std::string_view(in_string)) {}

Value::Value(std::string&& in_string) noexcept : data_(std::move(in_string)) {}

Value::Value(std::string_view in_string) : Value(std::string(in_string)) {}

Value::Value(const ArrayValue& in_array)
    : data_(base::ToVector(in_array, &Value::Clone)) {}

Value::Value(ArrayValue&& in_array) noexcept : data_(std::move(in_array)) {}

Value::Value(const MapValue& in_map)
    : data_(std::in_place_type<MapValue>,
            base::sorted_unique,
            base::ToVector(in_map, [](const auto& it) {
              return std::make_pair(it.first.Clone(), it.second.Clone());
            })) {}

Value::Value(MapValue&& in_map) noexcept : data_(std::move(in_map)) {}

Value& Value::operator=(Value&&) noexcept = default;

Value::~Value() = default;

Value Value::Clone() const {
  return Visit(absl::Overload{
      [](const InvalidUTF8& v) { return Value(invalid_utf8, v.bytes); },
      [](const auto& v) { return Value(v); },
  });
}

Value::Type Value::type() const {
  return Visit(absl::Overload{
      [](Null) { return Type::SIMPLE_VALUE; },
      [](Undefined) { return Type::SIMPLE_VALUE; },
      [](bool) { return Type::SIMPLE_VALUE; },
      [](int64_t value) {
        return value >= 0 ? Type::UNSIGNED : Type::NEGATIVE;
      },
      [](const BinaryValue&) { return Type::BYTE_STRING; },
      [](const std::string&) { return Type::STRING; },
      [](const ArrayValue&) { return Type::ARRAY; },
      [](const MapValue&) { return Type::MAP; },
      [](const InvalidUTF8&) { return Type::INVALID_UTF8; },
  });
}

Value::SimpleValue Value::GetSimpleValue() const {
  return Visit(absl::Overload{
      [](Null) { return SimpleValue::NULL_VALUE; },
      [](Undefined) { return SimpleValue::UNDEFINED; },
      [](bool value) {
        return value ? SimpleValue::TRUE_VALUE : SimpleValue::FALSE_VALUE;
      },
      [](const auto&) -> SimpleValue { NOTREACHED(); },
  });
}

bool Value::GetBool() const {
  return std::get<bool>(data_);
}

int64_t Value::GetInteger() const {
  return std::get<int64_t>(data_);
}

int64_t Value::GetUnsigned() const {
  CHECK(is_unsigned());
  return std::get<int64_t>(data_);
}

int64_t Value::GetNegative() const {
  CHECK(is_negative());
  return std::get<int64_t>(data_);
}

const std::string& Value::GetString() const {
  return std::get<std::string>(data_);
}

const Value::BinaryValue& Value::GetBytestring() const {
  return std::get<BinaryValue>(data_);
}

std::string_view Value::GetBytestringAsString() const {
  return base::as_string_view(GetBytestring());
}

const Value::ArrayValue& Value::GetArray() const {
  return std::get<ArrayValue>(data_);
}

Value::ArrayValue& Value::GetArray() {
  return std::get<ArrayValue>(data_);
}

const Value::MapValue& Value::GetMap() const {
  return std::get<MapValue>(data_);
}

Value::MapValue& Value::GetMap() {
  return std::get<MapValue>(data_);
}

const Value::BinaryValue& Value::GetInvalidUTF8() const {
  return std::get<InvalidUTF8>(data_).bytes;
}

}  // namespace cbor
