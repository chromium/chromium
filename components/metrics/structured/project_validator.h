// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_

#include <cstdint>
#include <string>

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
      const std::string& event_name);

  uint64_t project_hash() const;

 protected:
  // Should not be constructed directly.
  explicit ProjectValidator(uint64_t project_hash);

 private:
  const uint64_t project_hash_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_PROJECT_VALIDATOR_H_
