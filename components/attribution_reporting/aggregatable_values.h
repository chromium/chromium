// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableValuesValue {
 public:
  static std::optional<AggregatableValuesValue> Create(int value,
                                                       uint64_t filtering_id);

  static base::expected<AggregatableValuesValue,
                        mojom::TriggerRegistrationError>
  FromJSON(const base::Value&, mojom::TriggerRegistrationError);

  AggregatableValuesValue() = default;

  ~AggregatableValuesValue() = default;

  AggregatableValuesValue(const AggregatableValuesValue&) = default;
  AggregatableValuesValue& operator=(const AggregatableValuesValue&) = default;

  AggregatableValuesValue(AggregatableValuesValue&&) = default;
  AggregatableValuesValue& operator=(AggregatableValuesValue&&) = default;

  friend bool operator==(const AggregatableValuesValue&,
                         const AggregatableValuesValue&) = default;

  uint32_t value() const { return value_; }

  uint64_t filtering_id() const { return filtering_id_; }

  base::Value::Dict ToJson() const;

 private:
  AggregatableValuesValue(uint32_t value, uint64_t filtering_id);

  uint32_t value_;

  uint64_t filtering_id_;
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableValues {
 public:
  using Values = base::flat_map<std::string, AggregatableValuesValue>;

  static std::optional<AggregatableValues> Create(Values, FilterPair);

  static base::expected<std::vector<AggregatableValues>,
                        mojom::TriggerRegistrationError>
  FromJSON(base::Value*);

  AggregatableValues();

  ~AggregatableValues();

  AggregatableValues(const AggregatableValues&);
  AggregatableValues& operator=(const AggregatableValues&);

  AggregatableValues(AggregatableValues&&);
  AggregatableValues& operator=(AggregatableValues&&);

  const Values& values() const { return values_; }

  const FilterPair& filters() const { return filters_; }

  base::Value::Dict ToJson() const;

  friend bool operator==(const AggregatableValues&,
                         const AggregatableValues&) = default;

 private:
  AggregatableValues(Values, FilterPair);

  Values values_;

  // The filters used to determine whether the values can be used to create
  // contributions.
  FilterPair filters_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_
