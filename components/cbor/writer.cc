// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/writer.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "components/cbor/constants.h"

namespace cbor {

Writer::~Writer() = default;

// static
std::optional<std::vector<uint8_t>> Writer::Write(const Value& node,
                                                  const Config& config) {
  std::vector<uint8_t> cbor;
  Writer writer(&cbor);
  if (!writer.EncodeCBOR(node, config.max_nesting_level,
                         config.allow_invalid_utf8_for_testing)) {
    return std::nullopt;
  }
  return cbor;
}

// static
std::optional<std::vector<uint8_t>> Writer::Write(const Value& node,
                                                  size_t max_nesting_level) {
  Config config;
  config.max_nesting_level = base::checked_cast<int>(max_nesting_level);
  return Write(node, config);
}

Writer::Writer(std::vector<uint8_t>* cbor) : encoded_cbor_(cbor) {}

bool Writer::EncodeCBOR(const Value& node,
                        int max_nesting_level,
                        bool allow_invalid_utf8) {
  if (max_nesting_level < 0)
    return false;

  switch (node.type()) {
    case Value::Type::NONE: {
      StartItem(Value::Type::BYTE_STRING, 0);
      return true;
    }

    case Value::Type::INVALID_UTF8: {
      if (!allow_invalid_utf8) {
        NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
        return false;
      }
      // Encode a CBOR string with invalid UTF-8 data. This may produce invalid
      // CBOR and is reachable in tests only. See
      // |allow_invalid_utf8_for_testing| in Config.
      const Value::BinaryValue& bytes = node.GetInvalidUTF8();
      StartItem(Value::Type::STRING, base::strict_cast<uint64_t>(bytes.size()));
      encoded_cbor_->insert(encoded_cbor_->end(), bytes.begin(), bytes.end());
      return true;
    }

    // Represents unsigned integers.
    case Value::Type::UNSIGNED: {
      int64_t value = node.GetUnsigned();
      StartItem(Value::Type::UNSIGNED, static_cast<uint64_t>(value));
      return true;
    }

    // Represents negative integers.
    case Value::Type::NEGATIVE: {
      int64_t value = node.GetNegative();
      StartItem(Value::Type::NEGATIVE, static_cast<uint64_t>(-(value + 1)));
      return true;
    }

    // Represents a byte string.
    case Value::Type::BYTE_STRING: {
      const Value::BinaryValue& bytes = node.GetBytestring();
      StartItem(Value::Type::BYTE_STRING,
                base::strict_cast<uint64_t>(bytes.size()));
      // Add the bytes.
      encoded_cbor_->insert(encoded_cbor_->end(), bytes.begin(), bytes.end());
      return true;
    }

    case Value::Type::STRING: {
      std::string_view string = node.GetString();
      StartItem(Value::Type::STRING,
                base::strict_cast<uint64_t>(string.size()));

      // Add the characters.
      encoded_cbor_->insert(encoded_cbor_->end(), string.begin(), string.end());
      return true;
    }

    // Represents an array.
    case Value::Type::ARRAY: {
      const Value::ArrayValue& array = node.GetArray();
      StartItem(Value::Type::ARRAY, array.size());
      for (const auto& value : array) {
        if (!EncodeCBOR(value, max_nesting_level - 1, allow_invalid_utf8))
          return false;
      }
      return true;
    }

    // Represents a map.
    case Value::Type::MAP: {
      const Value::MapValue& map = node.GetMap();
      StartItem(Value::Type::MAP, map.size());

      for (const auto& value : map) {
        if (!EncodeCBOR(value.first, max_nesting_level - 1, allow_invalid_utf8))
          return false;
        if (!EncodeCBOR(value.second, max_nesting_level - 1,
                        allow_invalid_utf8))
          return false;
      }
      return true;
    }

    case Value::Type::TAG:
      NOTREACHED_IN_MIGRATION() << constants::kUnsupportedMajorType;
      return false;

    // Represents a simple value.
    case Value::Type::SIMPLE_VALUE: {
      const Value::SimpleValue simple_value = node.GetSimpleValue();
      StartItem(Value::Type::SIMPLE_VALUE,
                base::checked_cast<uint64_t>(simple_value));
      return true;
    }

    case Value::Type::FLOAT_VALUE: {
      const double float_value = node.GetDouble();
      encoded_cbor_->push_back(base::checked_cast<uint8_t>(
          static_cast<unsigned>(Value::Type::SIMPLE_VALUE)
          << constants::kMajorTypeBitShift));
      {
        uint16_t value_16 = EncodeHalfPrecisionFloat(float_value);
        const double decoded_float_16 = DecodeHalfPrecisionFloat(value_16);
        if (decoded_float_16 == float_value ||
            (std::isnan(decoded_float_16) && std::isnan(float_value))) {
          // We can encode it in 16 bits.

          SetAdditionalInformation(constants::kAdditionalInformation2Bytes);
          for (int shift = 1; shift >= 0; shift--) {
            encoded_cbor_->push_back(0xFF & (value_16 >> (shift * 8)));
          }
          return true;
        }
      }
      {
        const float float_value_32 = float_value;
        if (float_value == float_value_32) {
          // We can encode it in 32 bits.

          SetAdditionalInformation(constants::kAdditionalInformation4Bytes);
          uint32_t value_32 = base::bit_cast<uint32_t>(float_value_32);
          for (int shift = 3; shift >= 0; shift--) {
            encoded_cbor_->push_back(0xFF & (value_32 >> (shift * 8)));
          }
          return true;
        }
      }
      {
        // We can always encode it in 64 bits.
        SetAdditionalInformation(constants::kAdditionalInformation8Bytes);
        uint64_t value_64 = base::bit_cast<uint64_t>(float_value);
        for (int shift = 7; shift >= 0; shift--) {
          encoded_cbor_->push_back(0xFF & (value_64 >> (shift * 8)));
        }
        return true;
      }
    }
  }
}

void Writer::StartItem(Value::Type type, uint64_t size) {
  encoded_cbor_->push_back(base::checked_cast<uint8_t>(
      static_cast<unsigned>(type) << constants::kMajorTypeBitShift));
  SetUint(size);
}

void Writer::SetAdditionalInformation(uint8_t additional_information) {
  DCHECK(!encoded_cbor_->empty());
  DCHECK_EQ(additional_information & constants::kAdditionalInformationMask,
            additional_information);
  encoded_cbor_->back() |=
      (additional_information & constants::kAdditionalInformationMask);
}

void Writer::SetUint(uint64_t value) {
  size_t count = GetNumUintBytes(value);
  int shift = -1;
  // Values under 24 are encoded directly in the initial byte.
  // Otherwise, the last 5 bits of the initial byte contains the length
  // of unsigned integer, which is encoded in following bytes.
  switch (count) {
    case 0:
      SetAdditionalInformation(base::checked_cast<uint8_t>(value));
      break;
    case 1:
      SetAdditionalInformation(constants::kAdditionalInformation1Byte);
      shift = 0;
      break;
    case 2:
      SetAdditionalInformation(constants::kAdditionalInformation2Bytes);
      shift = 1;
      break;
    case 4:
      SetAdditionalInformation(constants::kAdditionalInformation4Bytes);
      shift = 3;
      break;
    case 8:
      SetAdditionalInformation(constants::kAdditionalInformation8Bytes);
      shift = 7;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  for (; shift >= 0; shift--) {
    encoded_cbor_->push_back(0xFF & (value >> (shift * 8)));
  }
}

size_t Writer::GetNumUintBytes(uint64_t value) {
  if (value < 24) {
    return 0;
  } else if (value <= 0xFF) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 2;
  } else if (value <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

}  // namespace cbor
