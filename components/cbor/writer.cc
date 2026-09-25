// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/writer.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/cbor/cbor_buildflags.h"
#include "components/cbor/constants.h"
#include "components/cbor/experiment_metrics.h"

#if BUILDFLAG(USE_CBOR_RUST)
#include "components/cbor/rust/cbor_rust.h"
#endif

namespace cbor {

BASE_FEATURE(kUseRustCborWriter, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Records `CBOR.Write.*` metrics on destruction.
class [[nodiscard]] ScopedMetricsReporter {
 public:
  explicit ScopedMetricsReporter(
      const std::optional<size_t>& output_size LIFETIME_BOUND)
      : output_size_(output_size) {}
  explicit ScopedMetricsReporter(std::optional<size_t>&&) = delete;

  ScopedMetricsReporter(const ScopedMetricsReporter&) = delete;
  ScopedMetricsReporter& operator=(const ScopedMetricsReporter&) = delete;

  ~ScopedMetricsReporter() {
    const base::TimeDelta elapsed = timer_.Elapsed();

    UMA_HISTOGRAM_BOOLEAN("CBOR.Write.Success", output_size_->has_value());
    if (output_size_->has_value()) {
      UMA_HISTOGRAM_COUNTS_10M("CBOR.Write.Size",
                               base::saturated_cast<int>(**output_size_));
    }
    if (base::TimeTicks::IsHighResolution()) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("CBOR.Write.Duration", elapsed,
                                              base::Microseconds(1),
                                              base::Milliseconds(100), 50);
    }
  }

 private:
  const base::raw_ref<const std::optional<size_t>> output_size_;
  const base::ElapsedTimer timer_;
};

// Resolves `Writer::Config::use_rust`, defaulting to `kUseRustCborWriter`.
bool ShouldUseRustWriter(std::optional<bool> use_rust) {
  if (use_rust.has_value()) {
    return *use_rust;
  }
#if BUILDFLAG(USE_CBOR_RUST)
  return base::FeatureList::IsEnabled(kUseRustCborWriter);
#else
  return false;
#endif
}

#if BUILDFLAG(USE_CBOR_RUST)
// Converts `node` (or map `key`) into a `cbor::rust::Value` / `MapKey`
// borrowing string and byte payloads from `node`. The returned value must not
// outlive `node`.
std::optional<cbor::rust::MapKey> ConvertCppMapKeyToRust(
    const Value& key LIFETIME_BOUND,
    int max_nesting_level,
    bool allow_invalid_utf8) {
  if (max_nesting_level < 0) {
    return std::nullopt;
  }
  switch (key.type()) {
    case Value::Type::UNSIGNED:
      return cbor::rust::MapKey::MakeInt(key.GetUnsigned());
    case Value::Type::NEGATIVE:
      return cbor::rust::MapKey::MakeInt(key.GetNegative());
    case Value::Type::BYTE_STRING:
      return cbor::rust::MapKey::MakeBytestring(
          rs_std::SliceRef<const uint8_t>(key.GetBytestring()));
    case Value::Type::STRING: {
      auto str_ref = rs_std::StrRef::FromUtf8(key.GetString());
      CHECK(str_ref.has_value());
      return cbor::rust::MapKey::MakeString(*str_ref);
    }
    case Value::Type::INVALID_UTF8:
      if (!allow_invalid_utf8) {
        NOTREACHED() << constants::kUnsupportedMajorType;
      }
      return cbor::rust::MapKey::MakeInvalidUtf8(
          rs_std::SliceRef<const uint8_t>(key.GetInvalidUTF8()));
    case Value::Type::ARRAY:
    case Value::Type::MAP:
    case Value::Type::SIMPLE_VALUE:
      NOTREACHED();
  }
  NOTREACHED();
}

std::optional<cbor::rust::Value> ConvertCppValueToRust(
    const Value& node LIFETIME_BOUND,
    int max_nesting_level,
    bool allow_invalid_utf8) {
  if (max_nesting_level < 0) {
    return std::nullopt;
  }

  switch (node.type()) {
    case Value::Type::INVALID_UTF8:
      if (!allow_invalid_utf8) {
        NOTREACHED() << constants::kUnsupportedMajorType;
      }
      return cbor::rust::Value::MakeInvalidUtf8(
          rs_std::SliceRef<const uint8_t>(node.GetInvalidUTF8()));
    case Value::Type::UNSIGNED:
      return cbor::rust::Value::MakeInt(node.GetUnsigned());
    case Value::Type::NEGATIVE:
      return cbor::rust::Value::MakeInt(node.GetNegative());
    case Value::Type::BYTE_STRING:
      return cbor::rust::Value::MakeBytestring(
          rs_std::SliceRef<const uint8_t>(node.GetBytestring()));
    case Value::Type::STRING: {
      auto str_ref = rs_std::StrRef::FromUtf8(node.GetString());
      CHECK(str_ref.has_value());
      return cbor::rust::Value::MakeString(*str_ref);
    }
    case Value::Type::ARRAY: {
      const Value::ArrayValue& array = node.GetArray();
      rs_std::Vec<cbor::rust::Value> items =
          cbor::rust::vec_with_capacity_values(array.size());
      for (const Value& elem : array) {
        std::optional<cbor::rust::Value> converted = ConvertCppValueToRust(
            elem, max_nesting_level - 1, allow_invalid_utf8);
        if (!converted) {
          return std::nullopt;
        }
        cbor::rust::vec_push_value(items, *std::move(converted));
      }
      return cbor::rust::Value::MakeArray(std::move(items));
    }
    case Value::Type::MAP: {
      const Value::MapValue& map = node.GetMap();
      rs_std::Vec<cbor::rust::MapEntry> entries =
          cbor::rust::vec_with_capacity_entries(map.size());
      for (const auto& [cpp_key, cpp_value] : map) {
        std::optional<cbor::rust::MapKey> key = ConvertCppMapKeyToRust(
            cpp_key, max_nesting_level - 1, allow_invalid_utf8);
        if (!key) {
          return std::nullopt;
        }
        std::optional<cbor::rust::Value> value = ConvertCppValueToRust(
            cpp_value, max_nesting_level - 1, allow_invalid_utf8);
        if (!value) {
          return std::nullopt;
        }
        cbor::rust::vec_push_entry(
            entries, cbor::rust::MapEntry{*std::move(key), *std::move(value)});
      }
      // `Value::MapValue` is already sorted in canonical CBOR order.
      return cbor::rust::Value::MakeMap(
          cbor::rust::Map::from_sorted_vec_unchecked(std::move(entries)));
    }
    case Value::Type::SIMPLE_VALUE:
      switch (node.GetSimpleValue()) {
        case Value::SimpleValue::FALSE_VALUE:
          return cbor::rust::Value::MakeBoolean(false);
        case Value::SimpleValue::TRUE_VALUE:
          return cbor::rust::Value::MakeBoolean(true);
        case Value::SimpleValue::NULL_VALUE:
          return cbor::rust::Value::MakeNull();
        case Value::SimpleValue::UNDEFINED:
          return cbor::rust::Value::MakeUndefined();
      }
      NOTREACHED();
  }
  NOTREACHED();
}
#endif

}  // namespace

Writer::~Writer() = default;

// static
std::optional<std::vector<uint8_t>> Writer::Write(const Value& node,
                                                  const Config& config) {
  const bool use_rust = ShouldUseRustWriter(config.use_rust);

  // Declared before `reporter` so it outlives the destructor that reads it.
  std::optional<size_t> output_size;
  std::optional<ScopedMetricsReporter> reporter;
  if (internal::ShouldRecordMetrics(config.use_rust)) {
    reporter.emplace(output_size);
  }

#if BUILDFLAG(USE_CBOR_RUST)
  if (use_rust) {
    std::optional<cbor::rust::Value> rust_val = ConvertCppValueToRust(
        node, config.max_nesting_level, config.allow_invalid_utf8_for_testing);
    if (!rust_val) {
      return std::nullopt;
    }
    rs_std::Vec<uint8_t> out = cbor::rust::write(*rust_val);
    output_size = out.size();
    return std::vector<uint8_t>(out.begin(), out.end());
  }
#else
  CHECK(!use_rust) << "CBOR Rust writer is statically disabled in this build";
#endif

  std::vector<uint8_t> cbor;
  Writer writer(&cbor);
  if (!writer.EncodeCBOR(node, config.max_nesting_level,
                         config.allow_invalid_utf8_for_testing)) {
    return std::nullopt;
  }
  output_size = cbor.size();
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
    case Value::Type::INVALID_UTF8: {
      if (!allow_invalid_utf8) {
        NOTREACHED() << constants::kUnsupportedMajorType;
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

    // Represents a simple value.
    case Value::Type::SIMPLE_VALUE: {
      const Value::SimpleValue simple_value = node.GetSimpleValue();
      StartItem(Value::Type::SIMPLE_VALUE,
                base::checked_cast<uint64_t>(simple_value));
      return true;
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
      NOTREACHED();
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
