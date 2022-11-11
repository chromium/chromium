// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/filters.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace attribution_reporting {

class AggregatableTriggerData;
struct EventTriggerData;

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) TriggerRegistration {
 public:
  static absl::optional<TriggerRegistration> Create(
      url::Origin reporting_origin,
      Filters filters,
      Filters not_filters,
      absl::optional<uint64_t> debug_key,
      absl::optional<uint64_t> aggregatable_dedup_key,
      std::vector<EventTriggerData> event_triggers,
      std::vector<AggregatableTriggerData> aggregatable_trigger_data,
      AggregatableValues aggregatable_values,
      bool debug_reporting);

  ~TriggerRegistration();

  TriggerRegistration(const TriggerRegistration&);
  TriggerRegistration& operator=(const TriggerRegistration&);

  TriggerRegistration(TriggerRegistration&&);
  TriggerRegistration& operator=(TriggerRegistration&&);

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  const Filters& filters() const { return filters_; }

  const Filters& not_filters() const { return not_filters_; }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  absl::optional<uint64_t> aggregatable_dedup_key() const {
    return aggregatable_dedup_key_;
  }

  const std::vector<EventTriggerData>& event_triggers() const {
    return event_triggers_;
  }

  const std::vector<AggregatableTriggerData>& aggregatable_trigger_data()
      const {
    return aggregatable_trigger_data_;
  }

  const AggregatableValues& aggregatable_values() const {
    return aggregatable_values_;
  }

  bool debug_reporting() const { return debug_reporting_; }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

 private:
  TriggerRegistration();

  url::Origin reporting_origin_;
  Filters filters_;
  Filters not_filters_;
  absl::optional<uint64_t> debug_key_;
  absl::optional<uint64_t> aggregatable_dedup_key_;
  std::vector<EventTriggerData> event_triggers_;
  std::vector<AggregatableTriggerData> aggregatable_trigger_data_;
  AggregatableValues aggregatable_values_;
  bool debug_reporting_ = false;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TRIGGER_REGISTRATION_H_
