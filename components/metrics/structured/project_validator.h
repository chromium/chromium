// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event_validator.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

using EventType = StructuredEventProto_EventType;

// Validates project metadata and events against definitions in structured.xml.
class ProjectValidator final {
 public:
  constexpr ProjectValidator(uint64_t project_hash,
                             EventType event_type,
                             IdType id_type,
                             IdScope id_scope,
                             uint16_t key_rotation_period,
                             uint16_t name_offset,
                             uint8_t name_len,
                             uint16_t events_offset,
                             uint16_t events_count)
      : project_hash_(project_hash),
        event_type_(event_type),
        id_type_(id_type),
        id_scope_(id_scope),
        key_rotation_period_(key_rotation_period),
        name_offset_(name_offset),
        events_offset_(events_offset),
        events_count_(events_count),
        name_len_(name_len) {}

  // Should not be copied or moved.
  ProjectValidator(const ProjectValidator&) = delete;
  ProjectValidator& operator=(const ProjectValidator& other) = delete;

  ~ProjectValidator() = default;

  // Returns the event validator if |event_name| is a valid event for this
  // project.
  const EventValidator* GetEventValidator(std::string_view event_name) const;

  std::optional<std::string_view> GetEventName(uint64_t event_name_hash) const;

  uint64_t project_hash() const { return project_hash_; }
  IdType id_type() const { return id_type_; }
  IdScope id_scope() const { return id_scope_; }
  EventType event_type() const { return event_type_; }
  int key_rotation_period() const { return key_rotation_period_; }
  uint16_t name_offset() const { return name_offset_; }
  uint8_t name_len() const { return name_len_; }

 private:
  const uint64_t project_hash_;
  const EventType event_type_;
  const IdType id_type_;
  const IdScope id_scope_;
  const uint16_t key_rotation_period_;
  const uint16_t name_offset_;
  const uint16_t events_offset_;
  const uint16_t events_count_;
  const uint8_t name_len_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
