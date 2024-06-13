// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_

#include <optional>
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

namespace attribution_reporting {

struct FilterPair;

// TODO(apaseltiner): Consider making the value type a `base::flat_set` because
// there is no semantic benefit to duplicate values, making it wasteful to pass
// them around. Unfortunately, this is difficult to do because there is no
// `mojo::ArrayTraits` deserialization for `base::flat_set`.
using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

class FilterConfig;

using FiltersDisjunction = std::vector<FilterConfig>;

// Set on sources.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterData {
 public:
  static constexpr char kSourceTypeFilterKey[] = "source_type";

  // Filter data is not allowed to contain a `source_type` filter.
  //
  // Note: This method is called with data deserialized from Mojo and proto. In
  // both cases, the values will already be deduplicated if they were produced
  // by the corresponding Mojo/proto serialization code, but if the serialized
  // data is corrupted or deliberately modified, it could contain duplicate
  // values; those duplicates will be retained by this method and count toward
  // the value-cardinality limit. This is OK, as the `Matches()` logic still
  // works correctly even in the presence of duplicates, excessive values, and
  // unordered values, but we may wish to be stricter here (e.g. by performing
  // deduplication as part of this method's operation) in order to match the
  // equivalent behavior in `FromJSON()`. This will be easier to accomplish once
  // the value type of `FilterValues` is changed to `base::flat_set`.
  static std::optional<FilterData> Create(FilterValues);

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

  friend bool operator==(const FilterData&, const FilterData&) = default;

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

  friend bool operator==(const FilterPair&, const FilterPair&) = default;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterConfig {
 public:
  static constexpr char kLookbackWindowKey[] = "_lookback_window";
  static constexpr char kReservedKeyPrefix[] = "_";

  // If set, FilterConfig's `lookback_window` must be positive.
  static std::optional<FilterConfig> Create(
      FilterValues,
      std::optional<base::TimeDelta> lookback_window = std::nullopt);

  FilterConfig();
  ~FilterConfig();

  FilterConfig(const FilterConfig&);
  FilterConfig(FilterConfig&&);

  FilterConfig& operator=(const FilterConfig&);
  FilterConfig& operator=(FilterConfig&&);

  const std::optional<base::TimeDelta>& lookback_window() const {
    return lookback_window_;
  }

  const FilterValues& filter_values() const { return filter_values_; }

  friend bool operator==(const FilterConfig&, const FilterConfig&) = default;

 private:
  explicit FilterConfig(FilterValues,
                        std::optional<base::TimeDelta> lookback_window);
  std::optional<base::TimeDelta> lookback_window_;
  FilterValues filter_values_;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<FiltersDisjunction, mojom::TriggerRegistrationError>
FiltersFromJSONForTesting(base::Value* input_value);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Value::List ToJsonForTesting(const FiltersDisjunction& filters);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
