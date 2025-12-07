// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/multiple_event_model_writer.h"

#include "base/functional/bind.h"
#include "components/feature_engagement/internal/event_model.h"

namespace feature_engagement {

MultipleEventModelWriter::MultipleEventModelWriter(
    raw_ptr<EventModel> profile_event_model,
    raw_ptr<EventModel> device_event_model)
    : profile_event_model_(profile_event_model),
      device_event_model_(device_event_model) {}

MultipleEventModelWriter::~MultipleEventModelWriter() = default;

void MultipleEventModelWriter::IncrementEvent(const std::string& event_name,
                                              uint32_t current_day) {
  profile_event_model_->IncrementEvent(event_name, current_day);
  device_event_model_->IncrementEvent(event_name, current_day);
}

void MultipleEventModelWriter::ClearEvent(const std::string& event_name) {
  profile_event_model_->ClearEvent(event_name);
  device_event_model_->ClearEvent(event_name);
}

void MultipleEventModelWriter::IncrementSnooze(const std::string& event_name,
                                               uint32_t current_day,
                                               base::Time current_time) {
  profile_event_model_->IncrementSnooze(event_name, current_day, current_time);
  device_event_model_->IncrementSnooze(event_name, current_day, current_time);
}

void MultipleEventModelWriter::DismissSnooze(const std::string& event_name) {
  profile_event_model_->DismissSnooze(event_name);
  device_event_model_->DismissSnooze(event_name);
}

}  // namespace feature_engagement
