// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) EventTriggerValue {
 public:
  static base::expected<EventTriggerValue, mojom::TriggerRegistrationError>
  Parse(const base::Value::Dict&);

  EventTriggerValue() = default;

  // `CHECK()`s that the given value is non-zero.
  explicit EventTriggerValue(uint32_t);

  EventTriggerValue(const EventTriggerValue&) = default;
  EventTriggerValue& operator=(const EventTriggerValue&) = default;

  EventTriggerValue(EventTriggerValue&&) = default;
  EventTriggerValue& operator=(EventTriggerValue&&) = default;

  // This implicit conversion is allowed to ease drop-in use of
  // this type in places currently requiring `uint32_t` with prior validation.
  operator uint32_t() const {  // NOLINT
    return value_;
  }

  void Serialize(base::Value::Dict&) const;

 private:
  uint32_t value_ = 1;
};

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) EventTriggerData {
  static base::expected<EventTriggerData, mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  // Data associated with trigger.
  // Will be sanitized to a lower entropy by the `AttributionStorageDelegate`
  // before storage.
  uint64_t data = 0;

  // Priority specified in conversion redirect. Used to prioritize which
  // reports to send among multiple different reports for the same attribution
  // source. Defaults to 0 if not provided.
  int64_t priority = 0;

  // Key specified in conversion redirect for deduplication against existing
  // conversions with the same source. If absent, no deduplication is
  // performed.
  std::optional<uint64_t> dedup_key;

  // The filters used to determine whether this `EventTriggerData'`s fields
  // are used.
  FilterPair filters;

  // TODO(crbug.com/40287976): Add an `EventTriggerValue` field called `value`.

  EventTriggerData();

  EventTriggerData(uint64_t data,
                   int64_t priority,
                   std::optional<uint64_t> dedup_key,
                   FilterPair);

  base::Value::Dict ToJson() const;

  friend bool operator==(const EventTriggerData&,
                         const EventTriggerData&) = default;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_EVENT_TRIGGER_DATA_H_
