// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/event_trigger_data.h"

#include <utility>

namespace attribution_reporting {

EventTriggerData::EventTriggerData(uint64_t data,
                                   int64_t priority,
                                   absl::optional<uint64_t> dedup_key,
                                   Filters filters,
                                   Filters not_filters)
    : data(data),
      priority(priority),
      dedup_key(dedup_key),
      filters(std::move(filters)),
      not_filters(std::move(not_filters)) {}

}  // namespace attribution_reporting
