// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_validator.h"

#include <cstdint>

namespace metrics {
namespace structured {

EventValidator::EventValidator(uint64_t event_hash) : event_hash_(event_hash) {}
EventValidator::~EventValidator() = default;

uint64_t EventValidator::event_hash() const {
  return event_hash_;
}

}  // namespace structured
}  // namespace metrics
