// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

struct FilterPair;

using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

class FilterConfig;

using FiltersDisjunction = std::vector<FilterConfig>;

// Set on sources.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterData {
 public:
  static constexpr char kSourceTypeFilterKey[] = "source_type";

  // Filter data is not allowed to contain a `source_type` filter.
  static absl::optional<FilterData> Create(FilterValues);

  static base::expected<FilterData, mojom::SourceRegistrationError> FromJSON(
      base::Value*);

  FilterData();

  ~FilterData();

  FilterData(const FilterData&);
  FilterData(FilterData&&);

  FilterData& operator=(const FilterData&);
  FilterData& operator=(FilterData&&);

  const FilterValues& filter_values() const { return filter_values_; }

  base::Value::Dict ToJson() const;

  bool Matches(mojom::SourceType,
               const base::Time& source_time,
               const base::Time& trigger_time,
               const FilterPair&) const;

  bool MatchesForTesting(mojom::SourceType,
                         const base::Time& source_time,
                         const base::Time& trigger_time,
                         const FiltersDisjunction&,
                         bool negated) const;

 private:
  explicit FilterData(FilterValues);

  bool Matches(mojom::SourceType,
               const base::Time& source_time,
               const base::Time& trigger_time,
               const FiltersDisjunction&,
               bool negated) const;

  FilterValues filter_values_;
};

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterPair {
  FilterPair();
  FilterPair(FiltersDisjunction positive, FiltersDisjunction negative);
  ~FilterPair();

  FilterPair(const FilterPair&);
  FilterPair(FilterPair&&);

  FilterPair& operator=(const FilterPair&);
  FilterPair& operator=(FilterPair&&);

  FiltersDisjunction positive;
  FiltersDisjunction negative;

  // Destructively parses the `filters` and `not_filters` fields from the given
  // dict, if present.
  static base::expected<FilterPair, mojom::TriggerRegistrationError> FromJSON(
      base::Value::Dict&);

  void SerializeIfNotEmpty(base::Value::Dict&) const;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterConfig {
 public:
  static constexpr char kLookbackWindowKey[] = "_lookback_window";

  // If set, FilterConfig's `lookback_window` must be positive.
  static absl::optional<FilterConfig> Create(
      FilterValues,
      absl::optional<base::TimeDelta> lookback_window = absl::nullopt);

  FilterConfig();
  ~FilterConfig();

  FilterConfig(const FilterConfig&);
  FilterConfig(FilterConfig&&);

  FilterConfig& operator=(const FilterConfig&);
  FilterConfig& operator=(FilterConfig&&);

  const absl::optional<base::TimeDelta>& lookback_window() const {
    return lookback_window_;
  }
  const FilterValues& filter_values() const { return filter_values_; }

 private:
  explicit FilterConfig(FilterValues,
                        absl::optional<base::TimeDelta> lookback_window);
  absl::optional<base::TimeDelta> lookback_window_;
  FilterValues filter_values_;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<FiltersDisjunction, mojom::TriggerRegistrationError>
FiltersFromJSONForTesting(base::Value* input_value);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value::List ToJsonForTesting(const FiltersDisjunction& filters);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
