// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_debug_reporting_config.h"

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/debug_types.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::DebugDataType;

constexpr char kAggregatableDebugReporting[] = "aggregatable_debug_reporting";
constexpr char kBudget[] = "budget";
constexpr char kDebugData[] = "debug_data";
constexpr char kTypes[] = "types";

using ParseDebugDataTypeFunc =
    base::FunctionRef<base::expected<DebugDataType, ParseError>(
        std::string_view)>;

bool IsValueInRange(int value, std::optional<int> max_value) {
  int effective_max_value = max_value.value_or(kMaxAggregatableValue);
  CHECK_LE(effective_max_value, kMaxAggregatableValue);
  return value > 0 && value <= effective_max_value;
}

base::expected<int, ParseError> ParseValue(const base::Value::Dict& dict,
                                           std::optional<int> max_value) {
  const base::Value* value = dict.Find(kValue);
  if (!value) {
    return base::unexpected(ParseError());
  }

  ASSIGN_OR_RETURN(int int_value, ParseInt(*value));

  if (!IsValueInRange(int_value, max_value)) {
    return base::unexpected(ParseError());
  }
  return int_value;
}

base::expected<int, AggregatableDebugReportingConfigError> ParseBudget(
    const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kBudget);
  if (!value) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kBudgetInvalid);
  }

  ASSIGN_OR_RETURN(int int_value, ParseInt(*value), [](ParseError) {
    return AggregatableDebugReportingConfigError::kBudgetInvalid;
  });

  if (!IsAggregatableValueInRange(int_value)) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kBudgetInvalid);
  }
  return int_value;
}

base::expected<absl::uint128, ParseError> ParseKeyPiece(
    const base::Value::Dict& dict) {
  const base::Value* v = dict.Find(kKeyPiece);
  if (!v) {
    return base::unexpected(ParseError());
  }

  return ParseAggregationKeyPiece(*v);
}

base::expected<void, AggregatableDebugReportingConfigError>
ParseDebugDataElement(base::Value& elem,
                      AggregatableDebugReportingConfig::DebugData& data,
                      std::optional<AggregatableDebugReportingContribution>&
                          unspecified_contribution,
                      std::set<std::string>& unknown_types,
                      std::optional<int> max_value,
                      ParseDebugDataTypeFunc parse_debug_data_type) {
  base::Value::Dict* dict = elem.GetIfDict();
  if (!dict) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kDebugDataInvalid);
  }

  ASSIGN_OR_RETURN(
      absl::uint128 key_piece,
      ParseKeyPiece(*dict).transform_error([](ParseError) {
        return AggregatableDebugReportingConfigError::kDebugDataKeyPieceInvalid;
      }));

  ASSIGN_OR_RETURN(
      int int_value,
      ParseValue(*dict, max_value).transform_error([](ParseError) {
        return AggregatableDebugReportingConfigError::kDebugDataValueInvalid;
      }));

  base::Value::List* l = dict->FindList(kTypes);
  if (!l || l->empty()) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kDebugDataTypesInvalid);
  }

  std::optional<AggregatableDebugReportingContribution> contribution =
      AggregatableDebugReportingContribution::Create(key_piece, int_value);
  CHECK(contribution.has_value());

  for (base::Value& v : *l) {
    std::string* s = v.GetIfString();
    if (!s) {
      return base::unexpected(
          AggregatableDebugReportingConfigError::kDebugDataTypesInvalid);
    }
    if (*s == "unspecified") {
      if (unspecified_contribution.has_value()) {
        return base::unexpected(
            AggregatableDebugReportingConfigError::kDebugDataTypesInvalid);
      }
      unspecified_contribution.emplace(*contribution);
      continue;
    }
    if (auto type = parse_debug_data_type(*s); type.has_value()) {
      auto [_, inserted] = data.try_emplace(*type, *contribution);
      if (!inserted) {
        return base::unexpected(
            AggregatableDebugReportingConfigError::kDebugDataTypesInvalid);
      }
    } else {
      auto [_, inserted] = unknown_types.emplace(std::move(*s));
      if (!inserted) {
        return base::unexpected(
            AggregatableDebugReportingConfigError::kDebugDataTypesInvalid);
      }
    }
  }

  return base::ok();
}

base::expected<AggregatableDebugReportingConfig::DebugData,
               AggregatableDebugReportingConfigError>
ParseDebugData(base::Value::Dict& dict,
               std::optional<int> max_value,
               const DebugDataTypes& debug_data_types,
               ParseDebugDataTypeFunc parse_debug_data_type) {
  base::Value* value = dict.Find(kDebugData);
  if (!value) {
    return {};
  }

  base::Value::List* l = value->GetIfList();
  if (!l) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kDebugDataInvalid);
  }

  AggregatableDebugReportingConfig::DebugData data;
  std::optional<AggregatableDebugReportingContribution>
      unspecified_contribution;

  for (std::set<std::string> unknown_types; base::Value & v : *l) {
    RETURN_IF_ERROR(ParseDebugDataElement(v, data, unspecified_contribution,
                                          unknown_types, max_value,
                                          parse_debug_data_type));
  }

  if (unspecified_contribution.has_value()) {
    for (DebugDataType type : debug_data_types) {
      data.try_emplace(type, *unspecified_contribution);
    }
  }

  return data;
}

void SerializeConfig(base::Value::Dict& dict,
                     const AggregatableDebugReportingConfig& config) {
  dict.Set(kKeyPiece, HexEncodeAggregationKey(config.key_piece));

  if (config.aggregation_coordinator_origin.has_value()) {
    dict.Set(kAggregationCoordinatorOrigin,
             config.aggregation_coordinator_origin->Serialize());
  }

  if (!config.debug_data.empty()) {
    auto list = base::Value::List::with_capacity(config.debug_data.size());
    for (const auto& [type, contribution] : config.debug_data) {
      CHECK(base::IsValueInRangeForNumericType<int>(contribution.value()));

      list.Append(
          base::Value::Dict()
              .Set(kKeyPiece, HexEncodeAggregationKey(contribution.key_piece()))
              .Set(kValue, static_cast<int>(contribution.value()))
              .Set(kTypes,
                   base::Value::List().Append(SerializeDebugDataType(type))));
    }
    dict.Set(kDebugData, std::move(list));
  }
}

bool IsValid(int budget,
             const AggregatableDebugReportingConfig::DebugData& data) {
  if (!IsRemainingAggregatableBudgetInRange(budget)) {
    return false;
  }

  return base::ranges::all_of(data, [&](const auto& p) {
    return IsValueInRange(p.second.value(), budget);
  });
}

base::expected<AggregatableDebugReportingConfig,
               AggregatableDebugReportingConfigError>
ParseConfig(base::Value::Dict& dict,
            std::optional<int> max_value,
            const DebugDataTypes& debug_data_types,
            ParseDebugDataTypeFunc parse_debug_data_type) {
  ASSIGN_OR_RETURN(
      auto key_piece, ParseKeyPiece(dict).transform_error([](ParseError) {
        return AggregatableDebugReportingConfigError::kKeyPieceInvalid;
      }));
  ASSIGN_OR_RETURN(
      auto aggregation_coordinator_origin,
      ParseAggregationCoordinator(dict).transform_error([](ParseError) {
        return AggregatableDebugReportingConfigError::
            kAggregationCoordinatorOriginInvalid;
      }));
  ASSIGN_OR_RETURN(auto data, ParseDebugData(dict, max_value, debug_data_types,
                                             parse_debug_data_type));

  return AggregatableDebugReportingConfig(
      key_piece, std::move(data), std::move(aggregation_coordinator_origin));
}

base::expected<SourceAggregatableDebugReportingConfig,
               AggregatableDebugReportingConfigError>
ParseSourceConfig(base::Value::Dict& dict) {
  base::Value* v = dict.Find(kAggregatableDebugReporting);
  if (!v) {
    return SourceAggregatableDebugReportingConfig();
  }

  base::Value::Dict* d = v->GetIfDict();
  if (!d) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kRootInvalid);
  }

  ASSIGN_OR_RETURN(int budget, ParseBudget(*d));

  ASSIGN_OR_RETURN(auto config, ParseConfig(*d, budget, SourceDebugDataTypes(),
                                            &ParseSourceDebugDataType));

  auto source_config =
      SourceAggregatableDebugReportingConfig::Create(budget, std::move(config));
  CHECK(source_config.has_value());
  return *std::move(source_config);
}

base::expected<AggregatableDebugReportingConfig,
               AggregatableDebugReportingConfigError>
ParseTriggerConfig(base::Value::Dict& dict) {
  base::Value* v = dict.Find(kAggregatableDebugReporting);
  if (!v) {
    return AggregatableDebugReportingConfig();
  }

  base::Value::Dict* d = v->GetIfDict();
  if (!d) {
    return base::unexpected(
        AggregatableDebugReportingConfigError::kRootInvalid);
  }

  return ParseConfig(*d, /*max_value=*/std::nullopt, TriggerDebugDataTypes(),
                     &ParseTriggerDebugDataType);
}

}  // namespace

// static
std::optional<AggregatableDebugReportingContribution>
AggregatableDebugReportingContribution::Create(absl::uint128 key_piece,
                                               uint32_t value) {
  if (!IsValueInRange(value, /*max_value=*/std::nullopt)) {
    return std::nullopt;
  }
  return AggregatableDebugReportingContribution(key_piece, value);
}

AggregatableDebugReportingContribution::AggregatableDebugReportingContribution(
    absl::uint128 key_piece,
    uint32_t value)
    : key_piece_(key_piece), value_(value) {
  CHECK(IsValid());
}

bool AggregatableDebugReportingContribution::IsValid() const {
  return IsValueInRange(value_, /*max_value=*/std::nullopt);
}

absl::uint128 AggregatableDebugReportingContribution::key_piece() const {
  CHECK(IsValid());
  return key_piece_;
}

uint32_t AggregatableDebugReportingContribution::value() const {
  CHECK(IsValid());
  return value_;
}

// static
base::expected<AggregatableDebugReportingConfig,
               AggregatableDebugReportingConfigError>
AggregatableDebugReportingConfig::Parse(base::Value::Dict& dict) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionAggregatableDebugReporting)) {
    return AggregatableDebugReportingConfig();
  }

  auto parsed = ParseTriggerConfig(dict);
  if (!parsed.has_value()) {
    base::UmaHistogramEnumeration(
        "Conversions.AggregatableDebugReporting.TriggerRegistrationError",
        parsed.error());
  }
  return parsed;
}

AggregatableDebugReportingConfig::AggregatableDebugReportingConfig() = default;

AggregatableDebugReportingConfig::AggregatableDebugReportingConfig(
    absl::uint128 key_piece,
    DebugData debug_data,
    std::optional<SuitableOrigin> aggregation_coordinator_origin)
    : key_piece(key_piece),
      debug_data(std::move(debug_data)),
      aggregation_coordinator_origin(
          std::move(aggregation_coordinator_origin)) {}

AggregatableDebugReportingConfig::~AggregatableDebugReportingConfig() = default;

AggregatableDebugReportingConfig::AggregatableDebugReportingConfig(
    const AggregatableDebugReportingConfig&) = default;

AggregatableDebugReportingConfig::AggregatableDebugReportingConfig(
    AggregatableDebugReportingConfig&&) = default;

AggregatableDebugReportingConfig& AggregatableDebugReportingConfig::operator=(
    const AggregatableDebugReportingConfig&) = default;

AggregatableDebugReportingConfig& AggregatableDebugReportingConfig::operator=(
    AggregatableDebugReportingConfig&&) = default;

void AggregatableDebugReportingConfig::Serialize(
    base::Value::Dict& dict) const {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionAggregatableDebugReporting)) {
    return;
  }

  base::Value::Dict body;
  SerializeConfig(body, *this);
  dict.Set(kAggregatableDebugReporting, std::move(body));
}

// static
base::expected<SourceAggregatableDebugReportingConfig,
               AggregatableDebugReportingConfigError>
SourceAggregatableDebugReportingConfig::Parse(base::Value::Dict& dict) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionAggregatableDebugReporting)) {
    return SourceAggregatableDebugReportingConfig();
  }

  auto parsed = ParseSourceConfig(dict);
  if (!parsed.has_value()) {
    base::UmaHistogramEnumeration(
        "Conversions.AggregatableDebugReporting.SourceRegistrationError",
        parsed.error());
  }
  return parsed;
}

// static
std::optional<SourceAggregatableDebugReportingConfig>
SourceAggregatableDebugReportingConfig::Create(
    int budget,
    AggregatableDebugReportingConfig config) {
  if (!IsValid(budget, config.debug_data)) {
    return std::nullopt;
  }
  return SourceAggregatableDebugReportingConfig(budget, std::move(config));
}

SourceAggregatableDebugReportingConfig::
    SourceAggregatableDebugReportingConfig() = default;

SourceAggregatableDebugReportingConfig::SourceAggregatableDebugReportingConfig(
    int budget,
    AggregatableDebugReportingConfig config)
    : budget_(budget), config_(std::move(config)) {
  CHECK(IsValid(budget_, config_.debug_data));
}

SourceAggregatableDebugReportingConfig::
    ~SourceAggregatableDebugReportingConfig() = default;

SourceAggregatableDebugReportingConfig::SourceAggregatableDebugReportingConfig(
    const SourceAggregatableDebugReportingConfig&) = default;

SourceAggregatableDebugReportingConfig::SourceAggregatableDebugReportingConfig(
    SourceAggregatableDebugReportingConfig&&) = default;

SourceAggregatableDebugReportingConfig&
SourceAggregatableDebugReportingConfig::operator=(
    const SourceAggregatableDebugReportingConfig&) = default;

SourceAggregatableDebugReportingConfig&
SourceAggregatableDebugReportingConfig::operator=(
    SourceAggregatableDebugReportingConfig&&) = default;

void SourceAggregatableDebugReportingConfig::Serialize(
    base::Value::Dict& dict) const {
  // `budget_` is 0 when aggregatable debug reporting is not opted in.
  if (!base::FeatureList::IsEnabled(
          features::kAttributionAggregatableDebugReporting) ||
      budget_ == 0) {
    return;
  }

  base::Value::Dict body;
  body.Set(kBudget, budget_);
  SerializeConfig(body, config_);
  dict.Set(kAggregatableDebugReporting, std::move(body));
}

}  // namespace attribution_reporting
