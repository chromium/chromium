// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event_validator.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {

using EventType = StructuredEventProto_EventType;

// Interface to be implemented by codegen for every project to validate
// messages received by the structured metric service.
class ProjectValidator {
 public:
  // Should not be copied or moved.
  ProjectValidator(const ProjectValidator&) = delete;
  ProjectValidator& operator=(const ProjectValidator& other) = delete;

  virtual ~ProjectValidator();

  // Returns the event validator if |event_name| is a valid event for this
  // project.
  const EventValidator* GetEventValidator(std::string_view event_name) const;

  std::optional<std::string_view> GetEventName(uint64_t event_name_hash) const;

  uint64_t project_hash() const { return project_hash_; }
  IdType id_type() const { return id_type_; }
  IdScope id_scope() const { return id_scope_; }
  EventType event_type() const { return event_type_; }
  int key_rotation_period() const { return key_rotation_period_; }

 protected:
  // Should not be constructed directly.
  ProjectValidator(uint64_t project_hash,
                   IdType id_type,
                   IdScope id_scope,
                   EventType event_type,
                   int key_rotation_period);

  std::unordered_map<std::string_view, std::unique_ptr<EventValidator>>
      event_validators_;

  std::unordered_map<uint64_t, std::string_view> event_name_map_;

 private:
  const uint64_t project_hash_;
  const IdType id_type_;
  const IdScope id_scope_;
  const EventType event_type_;
  const int key_rotation_period_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
