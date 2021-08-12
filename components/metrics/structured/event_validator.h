// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_

#include <cstdint>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace structured {

// Interface to be implemented by codegen for every event to validate
// messages received by the structured metric service.
class EventValidator {
 public:
  // Should not be copied or moved.
  EventValidator(const EventValidator&) = delete;
  EventValidator& operator=(const EventValidator& other) = delete;

  virtual ~EventValidator();

  // Returns the event validator if |metric_name| is a valid metric for this
  // event. This method is virtual because a static constexpr map will be
  // defined within each event validator implementation.
  virtual absl::optional<uint64_t> GetMetricHash(
      const std::string& metric_name) const;

  uint64_t event_hash() const;

 protected:
  // Should not be constructed directly.
  explicit EventValidator(uint64_t event_hash);

 private:
  uint64_t event_hash_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
