// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace attribution_reporting {

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

 private:
  explicit FilterData(FilterValues);

  FilterValues filter_values_;
};

// Set on triggers.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) Filters {
 public:
  // Filters are allowed to contain a `source_type` filter.
  static absl::optional<Filters> Create(FilterValues);

  Filters();

  ~Filters();

  Filters(const Filters&);
  Filters(Filters&&);

  Filters& operator=(const Filters&);
  Filters& operator=(Filters&&);

  const FilterValues& filter_values() const { return filter_values_; }

 private:
  explicit Filters(FilterValues);

  FilterValues filter_values_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FILTERS_H_
