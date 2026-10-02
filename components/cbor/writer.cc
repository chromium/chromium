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
#include "third_party/abseil-cpp/absl/functional/overload.h"

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
    const int max_nesting_level,
    const bool allow_invalid_utf8) {
  if (max_nesting_level < 0) {
    return std::nullopt;
  }
  return key.Visit(absl::Overload{
      [](int64_t v) { return cbor::rust::MapKey::MakeInt(v); },
      [](const Value::BinaryValue& v) {
        return cbor::rust::MapKey::MakeBytestring(
            rs_std::SliceRef<const uint8_t>(v));
      },
      [](const std::string& v) {
        auto str_ref = rs_std::StrRef::FromUtf8(v);
        CHECK(str_ref.has_value());
        return cbor::rust::MapKey::MakeString(*str_ref);
      },
      [&](const Value::InvalidUTF8& v) {
        if (!allow_invalid_utf8) {
          NOTREACHED() << constants::kUnsupportedMajorType;
        }
        return cbor::rust::MapKey::MakeInvalidUtf8(
            rs_std::SliceRef<const uint8_t>(v.bytes));
      },
      [](const auto&) -> cbor::rust::MapKey { NOTREACHED(); },
  });
}

std::optional<cbor::rust::Value> ConvertCppValueToRust(
    const Value& node LIFETIME_BOUND,
    const int max_nesting_level,
    const bool allow_invalid_utf8) {
  if (max_nesting_level < 0) {
    return std::nullopt;
  }

  return node.Visit(absl::Overload{
      [&](const Value::InvalidUTF8& v) -> std::optional<cbor::rust::Value> {
        if (!allow_invalid_utf8) {
          NOTREACHED() << constants::kUnsupportedMajorType;
        }
        return cbor::rust::Value::MakeInvalidUtf8(
            rs_std::SliceRef<const uint8_t>(v.bytes));
      },
      [](int64_t v) -> std::optional<cbor::rust::Value> {
        return cbor::rust::Value::MakeInt(v);
      },
      [](const Value::BinaryValue& v) -> std::optional<cbor::rust::Value> {
        return cbor::rust::Value::MakeBytestring(
            rs_std::SliceRef<const uint8_t>(v));
      },
      [](const std::string& v) -> std::optional<cbor::rust::Value> {
        auto str_ref = rs_std::StrRef::FromUtf8(v);
        CHECK(str_ref.has_value());
        return cbor::rust::Value::MakeString(*str_ref);
      },
      [&](const Value::ArrayValue& array) -> std::optional<cbor::rust::Value> {
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
      },
      [&](const Value::MapValue& map) -> std::optional<cbor::rust::Value> {
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
              entries,
              cbor::rust::MapEntry{*std::move(key), *std::move(value)});
        }
        // `Value::MapValue` is already sorted in canonical CBOR order.
        return cbor::rust::Value::MakeMap(
            cbor::rust::Map::from_sorted_vec_unchecked(std::move(entries)));
      },
      [](bool v) -> std::optional<cbor::rust::Value> {
        return cbor::rust::Value::MakeBoolean(v);
      },
      [](Value::Null) -> std::optional<cbor::rust::Value> {
        return cbor::rust::Value::MakeNull();
      },
      [](Value::Undefined) -> std::optional<cbor::rust::Value> {
        return cbor::rust::Value::MakeUndefined();
      },
  });
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
                        const int max_nesting_level,
                        const bool allow_invalid_utf8) {
  if (max_nesting_level < 0)
    return false;

  return node.Visit(absl::Overload{
      [&](const Value::InvalidUTF8& v) {
        if (!allow_invalid_utf8) {
          NOTREACHED() << constants::kUnsupportedMajorType;
        }
        // Encode a CBOR string with invalid UTF-8 data. This may produce
        // invalid CBOR and is reachable in tests only. See
        // |allow_invalid_utf8_for_testing| in Config.
        const Value::BinaryValue& bytes = v.bytes;
        StartItem(Value::Type::STRING,
                  base::strict_cast<uint64_t>(bytes.size()));
        encoded_cbor_->insert(encoded_cbor_->end(), bytes.begin(), bytes.end());
        return true;
      },
      [&](int64_t v) {
        if (v >= 0) {
          StartItem(Value::Type::UNSIGNED, static_cast<uint64_t>(v));
        } else {
          StartItem(Value::Type::NEGATIVE, static_cast<uint64_t>(-(v + 1)));
        }
        return true;
      },
      [&](const Value::BinaryValue& bytes) {
        StartItem(Value::Type::BYTE_STRING,
                  base::strict_cast<uint64_t>(bytes.size()));
        // Add the bytes.
        encoded_cbor_->insert(encoded_cbor_->end(), bytes.begin(), bytes.end());
        return true;
      },
      [&](const std::string& string) {
        StartItem(Value::Type::STRING,
                  base::strict_cast<uint64_t>(string.size()));

        // Add the characters.
        encoded_cbor_->insert(encoded_cbor_->end(), string.begin(),
                              string.end());
        return true;
      },
      [&](const Value::ArrayValue& array) {
        StartItem(Value::Type::ARRAY, array.size());
        for (const auto& value : array) {
          if (!EncodeCBOR(value, max_nesting_level - 1, allow_invalid_utf8)) {
            return false;
          }
        }
        return true;
      },
      [&](const Value::MapValue& map) {
        StartItem(Value::Type::MAP, map.size());

        for (const auto& value : map) {
          if (!EncodeCBOR(value.first, max_nesting_level - 1,
                          allow_invalid_utf8)) {
            return false;
          }
          if (!EncodeCBOR(value.second, max_nesting_level - 1,
                          allow_invalid_utf8)) {
            return false;
          }
        }
        return true;
      },
      [&](bool v) {
        StartItem(
            Value::Type::SIMPLE_VALUE,
            base::checked_cast<uint64_t>(v ? Value::SimpleValue::TRUE_VALUE
                                           : Value::SimpleValue::FALSE_VALUE));
        return true;
      },
      [&](Value::Null) {
        StartItem(Value::Type::SIMPLE_VALUE,
                  base::checked_cast<uint64_t>(Value::SimpleValue::NULL_VALUE));
        return true;
      },
      [&](Value::Undefined) {
        StartItem(Value::Type::SIMPLE_VALUE,
                  base::checked_cast<uint64_t>(Value::SimpleValue::UNDEFINED));
        return true;
      },
  });
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
