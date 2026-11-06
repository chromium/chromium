// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/project_validator.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "base/types/to_address.h"
#include "components/metrics/structured/structured_metrics_validator.h"

namespace metrics::structured {

const EventValidator* ProjectValidator::GetEventValidator(
    std::string_view event_name) const {
  const auto events =
      validator::Validators::GetEvents(events_offset_, events_count_);
  const auto it = std::ranges::lower_bound(
      events, event_name, std::less{}, [](const EventValidator& event) {
        return validator::Validators::GetString(event.name_offset(),
                                                event.name_len());
      });
  if (it == events.end() ||
      validator::Validators::GetString(it->name_offset(), it->name_len()) !=
          event_name) {
    return nullptr;
  }
  return base::to_address(it);
}

std::optional<std::string_view> ProjectValidator::GetEventName(
    uint64_t event_name_hash) const {
  for (const EventValidator& event :
       validator::Validators::GetEvents(events_offset_, events_count_)) {
    if (event.event_hash() == event_name_hash) {
      return validator::Validators::GetString(event.name_offset(),
                                              event.name_len());
    }
  }
  return std::nullopt;
}

}  // namespace metrics::structured
