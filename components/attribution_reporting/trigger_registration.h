// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerRegistration {
 public:
  TriggerRegistration(SuitableOrigin reporting_origin,
                      Filters filters,
                      Filters not_filters,
                      absl::optional<uint64_t> debug_key,
                      absl::optional<uint64_t> aggregatable_dedup_key,
                      EventTriggerDataList event_triggers,
                      AggregatableTriggerDataList aggregatable_trigger_data,
                      AggregatableValues aggregatable_values,
                      bool debug_reporting);

  ~TriggerRegistration();

  TriggerRegistration(const TriggerRegistration&);
  TriggerRegistration& operator=(const TriggerRegistration&);

  TriggerRegistration(TriggerRegistration&&);
  TriggerRegistration& operator=(TriggerRegistration&&);

  SuitableOrigin reporting_origin;
  Filters filters;
  Filters not_filters;
  absl::optional<uint64_t> debug_key;
  absl::optional<uint64_t> aggregatable_dedup_key;
  EventTriggerDataList event_triggers;
  AggregatableTriggerDataList aggregatable_trigger_data;
  AggregatableValues aggregatable_values;
  bool debug_reporting = false;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
