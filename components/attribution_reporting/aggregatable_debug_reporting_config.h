// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORTING_CONFIG_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORTING_CONFIG_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/debug_types.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace attribution_reporting {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AggregatableDebugReportingConfigError)
enum AggregatableDebugReportingConfigError {
  kRootInvalid = 0,
  // This value only applies to source registrations.
  kBudgetInvalid = 1,
  kKeyPieceInvalid = 2,
  kDebugDataInvalid = 3,
  kDebugDataKeyPieceInvalid = 4,
  kDebugDataValueInvalid = 5,
  kDebugDataTypesInvalid = 6,
  kAggregationCoordinatorOriginInvalid = 7,
  kMaxValue = kAggregationCoordinatorOriginInvalid,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionAggregatableDebugReportingRegistrationError)

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    AggregatableDebugReportingContribution {
 public:
  static std::optional<AggregatableDebugReportingContribution> Create(
      absl::uint128 key_piece,
      uint32_t value);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  AggregatableDebugReportingContribution() = default;

  absl::uint128 key_piece() const;
  uint32_t value() const;

  friend bool operator==(const AggregatableDebugReportingContribution&,
                         const AggregatableDebugReportingContribution&) =
      default;

 private:
  AggregatableDebugReportingContribution(absl::uint128 key_piece,
                                         uint32_t value);

  bool IsValid() const;

  absl::uint128 key_piece_;
  uint32_t value_;
};

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    AggregatableDebugReportingConfig {
  using DebugData = base::flat_map<mojom::DebugDataType,
                                   AggregatableDebugReportingContribution>;

  // Parses the config for trigger registrations.
  static base::expected<AggregatableDebugReportingConfig,
                        AggregatableDebugReportingConfigError>
  Parse(base::Value::Dict&);

  AggregatableDebugReportingConfig(
      absl::uint128 key_piece,
      DebugData,
      std::optional<attribution_reporting::SuitableOrigin>
          aggregation_coordinator_origin);

  AggregatableDebugReportingConfig();
  ~AggregatableDebugReportingConfig();

  AggregatableDebugReportingConfig(const AggregatableDebugReportingConfig&);
  AggregatableDebugReportingConfig(AggregatableDebugReportingConfig&&);

  AggregatableDebugReportingConfig& operator=(
      const AggregatableDebugReportingConfig&);
  AggregatableDebugReportingConfig& operator=(
      AggregatableDebugReportingConfig&&);

  void Serialize(base::Value::Dict&) const;

  friend bool operator==(const AggregatableDebugReportingConfig&,
                         const AggregatableDebugReportingConfig&) = default;

  absl::uint128 key_piece = 0;
  DebugData debug_data;
  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    SourceAggregatableDebugReportingConfig {
 public:
  static base::expected<SourceAggregatableDebugReportingConfig,
                        AggregatableDebugReportingConfigError>
  Parse(base::Value::Dict&);

  static std::optional<SourceAggregatableDebugReportingConfig> Create(
      int budget,
      AggregatableDebugReportingConfig);

  SourceAggregatableDebugReportingConfig();
  ~SourceAggregatableDebugReportingConfig();

  SourceAggregatableDebugReportingConfig(
      const SourceAggregatableDebugReportingConfig&);
  SourceAggregatableDebugReportingConfig(
      SourceAggregatableDebugReportingConfig&&);

  SourceAggregatableDebugReportingConfig& operator=(
      const SourceAggregatableDebugReportingConfig&);
  SourceAggregatableDebugReportingConfig& operator=(
      SourceAggregatableDebugReportingConfig&&);

  int budget() const { return budget_; }

  const AggregatableDebugReportingConfig& config() const { return config_; }

  void Serialize(base::Value::Dict&) const;

  friend bool operator==(const SourceAggregatableDebugReportingConfig&,
                         const SourceAggregatableDebugReportingConfig&) =
      default;

 private:
  SourceAggregatableDebugReportingConfig(int budget,
                                         AggregatableDebugReportingConfig);

  int budget_ = 0;
  AggregatableDebugReportingConfig config_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEBUG_REPORTING_CONFIG_H_
