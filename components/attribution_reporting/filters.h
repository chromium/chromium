// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class Filters;

struct FilterPair;

using FilterValues = base::flat_map<std::string, std::vector<std::string>>;

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

  bool Matches(mojom::SourceType, const FilterPair&) const;

  bool MatchesForTesting(mojom::SourceType, const Filters&, bool negated) const;

 private:
  explicit FilterData(FilterValues);

  bool Matches(mojom::SourceType, const Filters&, bool negated) const;

  FilterValues filter_values_;
};

// Set on triggers.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) Filters {
 public:
  // Filters are allowed to contain a `source_type` filter.
  static absl::optional<Filters> Create(FilterValues);

  static base::expected<Filters, mojom::TriggerRegistrationError> FromJSON(
      base::Value*);

  // Returns filters that match only the given source type.
  static Filters ForSourceTypeForTesting(mojom::SourceType);

  Filters();

  ~Filters();

  Filters(const Filters&);
  Filters(Filters&&);

  Filters& operator=(const Filters&);
  Filters& operator=(Filters&&);

  const FilterValues& filter_values() const { return filter_values_; }

  base::Value::Dict ToJson() const;

  void SerializeIfNotEmpty(base::Value::Dict&, base::StringPiece key) const;

 private:
  explicit Filters(FilterValues);

  FilterValues filter_values_;
};

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) FilterPair {
  Filters positive;
  Filters negative;

  // Destructively parses the `filters` and `not_filters` fields from the given
  // dict, if present.
  static base::expected<FilterPair, mojom::TriggerRegistrationError> FromJSON(
      base::Value::Dict&);

  void SerializeIfNotEmpty(base::Value::Dict&) const;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
