// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_READER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_READER_H_

#include <map>
#include <string>

#include "base/time/time.h"

namespace feature_engagement {
class Event;

// A EventModelReader provides all necessary runtime state.
class EventModelReader {
 public:
  virtual ~EventModelReader() = default;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Retrieves the Event object for the event with the given name. If the event
  // is not found, a nullptr will be returned. Calling this before the
  // EventModelReader has finished initializing will result in undefined
  // behavior.
  virtual const Event* GetEvent(const std::string& event_name) const = 0;

  // Returns the number of times the event with |event_name| happened in the
  // past |window_size| days from and including |current_day|, subtracted by
  // the number of times the event has been snoozed.
  // If |window_size| > |current_day|, all events since UNIX epoch will be
  // counted, since |current_day| represents number of days since UNIX epoch.
  virtual uint32_t GetEventCount(const std::string& event_name,
                                 uint32_t current_day,
                                 uint32_t window_size) const = 0;

  // Returns the last snooze timestamp for the feature associated with
  // |event_name|.
  virtual base::Time GetLastSnoozeTimestamp(
      const std::string& event_name) const = 0;

  // Returns the snooze count. Used for comparison against the max limit.
  virtual uint32_t GetSnoozeCount(const std::string& event_name,
                                  uint32_t window,
                                  uint32_t current_day) const = 0;

  // Returns whether the user has dismissed the snooze associated with
  // |event_name|.
  virtual bool IsSnoozeDismissed(const std::string& event_name) const = 0;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_READER_H_
