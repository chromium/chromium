// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/project_validator.h"

#include <cstdint>
#include <string_view>

#include "components/metrics/structured/enums.h"
#include "project_validator.h"

namespace metrics::structured {

ProjectValidator::ProjectValidator(uint64_t project_hash,
                                   IdType id_type,
                                   IdScope id_scope,
                                   EventType event_type,
                                   int key_rotation_period)
    : project_hash_(project_hash),
      id_type_(id_type),
      id_scope_(id_scope),
      event_type_(event_type),
      key_rotation_period_(key_rotation_period) {}

ProjectValidator::~ProjectValidator() = default;

const EventValidator* ProjectValidator::GetEventValidator(
    std::string_view event_name) const {
  const auto it = event_validators_.find(event_name);
  if (it == event_validators_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::optional<std::string_view> ProjectValidator::GetEventName(
    uint64_t event_name_hash) const {
  const auto it = event_name_map_.find(event_name_hash);
  if (it == event_name_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace metrics::structured
