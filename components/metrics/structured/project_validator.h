// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_

#include <cstdint>
#include <string>

#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace structured {

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
  virtual absl::optional<const EventValidator*> GetEventValidator(
      const std::string& event_name) const = 0;

  uint64_t project_hash() const { return project_hash_; }
  IdType id_type() const { return id_type_; }
  IdScope id_scope() const { return id_scope_; }
  EventType event_type() const { return event_type_; }

 protected:
  // Should not be constructed directly.
  ProjectValidator(uint64_t project_hash,
                   IdType id_type,
                   IdScope id_scope,
                   EventType event_type);

 private:
  const uint64_t project_hash_;
  const IdType id_type_;
  const IdScope id_scope_;
  const EventType event_type_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
