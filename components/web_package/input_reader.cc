// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/input_reader.h"

#include "base/containers/contains.h"
#include "base/functional/overloaded.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/cbor/constants.h"
#include "components/cbor/values.h"

namespace web_package {

namespace {

// This array must be kept in sync with the `CBORType` enum.
constexpr std::array kAcceptedCBORTypes = {
    // clang-format off
    CBORType::kUnsignedInt,
    CBORType::kNegativeInt,
    CBORType::kByteString,
    CBORType::kTextString,
    CBORType::kArray,
    CBORType::kMap,
    CBORType::kSimpleValue,
    // clang-format on
};

std::optional<int64_t> DecodeValueToNegative(uint64_t value) {
  auto negative_value = -base::CheckedNumeric<int64_t>(value) - 1;
  if (!negative_value.IsValid()) {
    return std::nullopt;
  }
  return static_cast<int64_t>(negative_value.ValueOrDie());
}

std::optional<int64_t> DecodeValueToUnsigned(uint64_t value) {
  auto unsigned_value = base::CheckedNumeric<int64_t>(value);
  if (!unsigned_value.IsValid()) {
    return std::nullopt;
  }
  return static_cast<int64_t>(unsigned_value.ValueOrDie());
}

}  // namespace

InputReader::InputReader(base::span<const uint8_t> buf) : buf_(buf) {}

InputReader::~InputReader() = default;

std::optional<uint8_t> InputReader::ReadByte() {
  uint8_t b;
  if (!buf_.ReadU8BigEndian(b)) {
    return std::nullopt;
  }
  return {b};
}

std::optional<base::span<const uint8_t>> InputReader::ReadBytes(size_t n) {
  return buf_.Read(n);
}

std::optional<std::string_view> InputReader::ReadString(size_t n) {
  auto bytes = buf_.Read(n);
  if (!bytes) {
    return std::nullopt;
  }
  if (!base::IsStringUTF8(base::as_string_view(*bytes))) {
    return std::nullopt;
  }
  return base::as_string_view(*bytes);
}

std::optional<uint64_t> InputReader::ReadCBORHeader(CBORType expected_type) {
  auto pair = ReadTypeAndArgument();
  if (!pair || pair->first != expected_type) {
    return std::nullopt;
  }
  return pair->second;
}

std::optional<CBORHeader> InputReader::ReadCBORHeader() {
  auto pair = ReadTypeAndArgument();
  if (!pair) {
    return std::nullopt;
  }

  const auto& [type, additional_info] = *pair;
  switch (type) {
    case CBORType::kSimpleValue: {
      using SimpleValue = cbor::Value::SimpleValue;
      if (additional_info == base::to_underlying(SimpleValue::TRUE_VALUE)) {
        return {{true}};
      } else if (additional_info ==
                 base::to_underlying(SimpleValue::FALSE_VALUE)) {
        return {{false}};
      } else {
        return std::nullopt;
      }
    }
    case CBORType::kUnsignedInt:
    case CBORType::kNegativeInt: {
      std::optional<int64_t> value =
          type == CBORType::kUnsignedInt
              ? DecodeValueToUnsigned(additional_info)
              : DecodeValueToNegative(additional_info);
      if (!value) {
        return std::nullopt;
      }
      return {{*value}};
    }
    case CBORType::kByteString:
    case CBORType::kTextString: {
      using StringInfo = CBORHeader::StringInfo;
      using StringType = CBORHeader::StringInfo::StringType;

      return {{StringInfo{.type = type == CBORType::kByteString
                                      ? StringType::kByteString
                                      : StringType::kTextString,
                          .byte_length = additional_info}}};
    }
    case CBORType::kArray:
    case CBORType::kMap: {
      using ContainerInfo = CBORHeader::ContainerInfo;
      using ContainerType = CBORHeader::ContainerInfo::ContainerType;

      return {{ContainerInfo{.type = type == CBORType::kArray
                                         ? ContainerType::kArray
                                         : ContainerType::kMap,
                             .size = additional_info}}};
    }
  }
}

// https://datatracker.ietf.org/doc/html/rfc8949.html#section-3
std::optional<std::pair<CBORType, uint64_t>>
InputReader::ReadTypeAndArgument() {
  std::optional<uint8_t> first_byte = ReadByte();
  if (!first_byte) {
    return std::nullopt;
  }

  // There are more CBOR types in the standard than we accept. To avoid mishits
  // during `static_cast<CBORType>`, it's safer to validate the type beforehand.
  uint8_t type_byte = (*first_byte & cbor::constants::kMajorTypeMask) >>
                      cbor::constants::kMajorTypeBitShift;
  if (!base::Contains(kAcceptedCBORTypes, type_byte,
                      &base::to_underlying<CBORType>)) {
    return std::nullopt;
  }

  CBORType type = static_cast<CBORType>(type_byte);
  uint8_t b = *first_byte & cbor::constants::kAdditionalInformationMask;

  if (b <= 23) {
    return std::make_pair(type, b);
  }
  if (b == 24) {
    auto content = ReadByte();
    if (!content || *content < 24) {
      return std::nullopt;
    }
    return std::make_pair(type, *content);
  }
  if (b == 25) {
    uint16_t content;
    if (!ReadBigEndian(&content) || content >> 8 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  if (b == 26) {
    uint32_t content;
    if (!ReadBigEndian(&content) || content >> 16 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  if (b == 27) {
    uint64_t content;
    if (!ReadBigEndian(&content) || content >> 32 == 0) {
      return std::nullopt;
    }
    return std::make_pair(type, content);
  }
  return std::nullopt;
}

}  // namespace web_package
