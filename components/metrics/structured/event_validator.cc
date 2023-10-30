// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_validator.h"

#include <cstdint>
#include "event_validator.h"

namespace metrics {
namespace structured {

EventValidator::EventValidator(uint64_t event_hash, bool force_record)
    : event_hash_(event_hash), force_record_(force_record) {}
EventValidator::~EventValidator() = default;

uint64_t EventValidator::event_hash() const {
  return event_hash_;
}

bool EventValidator::can_force_record() const {
  return force_record_;
}

}  // namespace structured
}  // namespace metrics
