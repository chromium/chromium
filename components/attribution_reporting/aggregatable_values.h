// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableValues {
 public:
  using Values = base::flat_map<std::string, uint32_t>;

  static absl::optional<AggregatableValues> Create(Values);

  static base::expected<AggregatableValues, mojom::TriggerRegistrationError>
  FromJSON(const base::Value*);

  AggregatableValues();

  ~AggregatableValues();

  AggregatableValues(const AggregatableValues&);
  AggregatableValues& operator=(const AggregatableValues&);

  AggregatableValues(AggregatableValues&&);
  AggregatableValues& operator=(AggregatableValues&&);

  const Values& values() const { return values_; }

  base::Value::Dict ToJson() const;

 private:
  explicit AggregatableValues(Values);

  Values values_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_VALUES_H_
