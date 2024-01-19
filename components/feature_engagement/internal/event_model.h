// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace feature_engagement {
class Event;

// A EventModel provides all necessary runtime state.
class EventModel {
 public:
  // Callback for when model initialization has finished. The |success|
  // argument denotes whether the model was successfully initialized.
  using OnModelInitializationFinished = base::OnceCallback<void(bool success)>;

  EventModel(const EventModel&) = delete;
  EventModel& operator=(const EventModel&) = delete;

  virtual ~EventModel() = default;

  // Initialize the model, including all underlying sub systems. When all
  // required operations have been finished, a callback is posted.
  virtual void Initialize(OnModelInitializationFinished callback,
                          uint32_t current_day) = 0;

  // Returns whether the model is ready, i.e. whether it has been successfully
  // initialized.
  virtual bool IsReady() const = 0;

  // Retrieves the Event object for the event with the given name. If the event
  // is not found, a nullptr will be returned. Calling this before the
  // EventModel has finished initializing will result in undefined behavior.
  virtual const Event* GetEvent(const std::string& event_name) const = 0;

  // Returns the number of times the event with |event_name| happened in the
  // past |window_size| days from and including |current_day|, subtracted by
  // the number of times the event has been snoozed.
  // If |window_size| > |current_day|, all events since UNIX epoch will be
  // counted, since |current_day| represents number of days since UNIX epoch.
  virtual uint32_t GetEventCount(const std::string& event_name,
                                 uint32_t current_day,
                                 uint32_t window_size) const = 0;

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

 protected:
  EventModel() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_H_
