// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_WRITER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_WRITER_H_

#include <map>
#include <string>

#include "base/time/time.h"

namespace feature_engagement {
class Event;

// A EventModelWriter provides all necessary runtime state.
class EventModelWriter {
 public:
  virtual ~EventModelWriter() = default;

  // Increments the counter for today for how many times the event has happened.
  // If the event has never happened before, the Event object will be created.
  // The |current_day| should be the number of days since UNIX epoch (see
  // TimeProvider::GetCurrentDay()).
  virtual void IncrementEvent(const std::string& event_name,
                              uint32_t current_day) = 0;

  // Removes data associated with `event_name`.
  virtual void ClearEvent(const std::string& event_name) = 0;

  // Increments the snooze count for the day.
  // Updates the last_snooze_time_us.
  virtual void IncrementSnooze(const std::string& event_name,
                               uint32_t current_day,
                               base::Time current_time) = 0;

  // Sets the snooze_dismissed flag.
  virtual void DismissSnooze(const std::string& event_name) = 0;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_WRITER_H_
