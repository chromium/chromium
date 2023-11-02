// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_user_agent_override.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace sessions {

SerializedUserAgentOverride::SerializedUserAgentOverride() = default;
SerializedUserAgentOverride::SerializedUserAgentOverride(
    const SerializedUserAgentOverride& other) = default;
SerializedUserAgentOverride::SerializedUserAgentOverride(
    SerializedUserAgentOverride&& other) = default;

SerializedUserAgentOverride& SerializedUserAgentOverride::operator=(
    const SerializedUserAgentOverride& other) = default;
SerializedUserAgentOverride& SerializedUserAgentOverride::operator=(
    SerializedUserAgentOverride&& other) = default;

SerializedUserAgentOverride::~SerializedUserAgentOverride() = default;

size_t SerializedUserAgentOverride::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(ua_string_override) +
         (opaque_ua_metadata_override.has_value()
              ? base::trace_event::EstimateMemoryUsage(
                    opaque_ua_metadata_override.value())
              : 0u);
}

}  // namespace sessions
