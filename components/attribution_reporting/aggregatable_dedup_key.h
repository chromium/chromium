// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEDUP_KEY_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEDUP_KEY_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableDedupKey {
  static base::expected<AggregatableDedupKey, mojom::TriggerRegistrationError>
  FromJSON(base::Value& value);

  // Key specified for deduplication against existing trigger with the same
  // source. If absent, no deduplication is performed.
  std::optional<uint64_t> dedup_key;

  // The filters used to determine whether this `AggregatableDedupKey`'s dedup
  // key is used.
  FilterPair filters;

  AggregatableDedupKey();

  AggregatableDedupKey(std::optional<uint64_t> dedup_key, FilterPair);

  base::Value::Dict ToJson() const;

  friend bool operator==(const AggregatableDedupKey&,
                         const AggregatableDedupKey&) = default;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_DEDUP_KEY_H_
