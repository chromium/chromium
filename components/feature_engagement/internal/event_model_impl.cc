// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_model_impl.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/event_storage_validator.h"
#include "components/feature_engagement/internal/event_store.h"

namespace feature_engagement {

EventModelImpl::EventModelImpl(
    std::unique_ptr<EventStore> store,
    std::unique_ptr<EventStorageValidator> storage_validator)
    : store_(std::move(store)),
      storage_validator_(std::move(storage_validator)),
      ready_(false) {}

EventModelImpl::~EventModelImpl() = default;

void EventModelImpl::Initialize(OnModelInitializationFinished callback,
                                uint32_t current_day) {
  store_->Load(base::BindOnce(&EventModelImpl::OnStoreLoaded,
                              weak_factory_.GetWeakPtr(), std::move(callback),
                              current_day));
}

bool EventModelImpl::IsReady() const {
  return ready_;
}

const Event* EventModelImpl::GetEvent(const std::string& event_name) const {
  auto search = events_.find(event_name);
  if (search == events_.end())
    return nullptr;

  return &search->second;
}

uint32_t EventModelImpl::GetEventCount(const std::string& event_name,
                                       uint32_t current_day,
                                       uint32_t window_size) const {
  uint32_t event_count =
      GetEventCountOrSnooze(event_name, current_day, window_size,
                            /*is_snooze=*/false);
  uint32_t snooze_count =
      GetEventCountOrSnooze(event_name, current_day, window_size,
                            /*is_snooze=*/true);
  return event_count - snooze_count;
}

void EventModelImpl::IncrementEvent(const std::string& event_name,
                                    uint32_t current_day) {
  DCHECK(ready_);

  if (!storage_validator_->ShouldStore(event_name)) {
    DVLOG(2) << "Not incrementing event " << event_name << " @ " << current_day;
    return;
  }

  DVLOG(2) << "Incrementing event " << event_name << " @ " << current_day;

  Event& event = GetNonConstEvent(event_name);
  for (int i = 0; i < event.events_size(); ++i) {
    Event_Count* event_count = event.mutable_events(i);
    DCHECK(event_count->has_day());
    DCHECK(event_count->has_count());
    if (event_count->day() == current_day) {
      event_count->set_count(event_count->count() + 1);
      store_->WriteEvent(event);
      return;
    }
  }

  // Day not found for event, adding new day with a count of 1.
  Event_Count* event_count = event.add_events();
  event_count->set_day(current_day);
  event_count->set_count(1u);
  store_->WriteEvent(event);
}

void EventModelImpl::ClearEvent(const std::string& event_name) {
  DCHECK(ready_);

  Event& event = GetNonConstEvent(event_name);
  event.clear_events();
  store_->WriteEvent(event);
}

void EventModelImpl::IncrementSnooze(const std::string& event_name,
                                     uint32_t current_day,
                                     base::Time current_time) {
  DCHECK(ready_);

  DVLOG(2) << "Incrementing snooze for event  " << event_name << " on "
           << current_day << " @ " << current_time;

  Event& event = GetNonConstEvent(event_name);
  for (int i = 0; i < event.events_size(); ++i) {
    Event_Count* event_count = event.mutable_events(i);
    DCHECK(event_count->has_day());
    DCHECK(event_count->has_count());
    if (event_count->day() != current_day)
      continue;
    event_count->set_snooze_count(
        event_count->has_snooze_count() ? event_count->snooze_count() + 1 : 1u);
  }
  event.set_last_snooze_time_us(
      current_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  store_->WriteEvent(event);
}

void EventModelImpl::DismissSnooze(const std::string& event_name) {
  DCHECK(ready_);
  Event& event = GetNonConstEvent(event_name);
  event.set_snooze_dismissed(true);
  store_->WriteEvent(event);
}

base::Time EventModelImpl::GetLastSnoozeTimestamp(
    const std::string& event_name) const {
  const Event* event = GetEvent(event_name);

  // If the Event object is not found, return.
  if (!event)
    return base::Time();

  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(event->last_snooze_time_us()));
}

uint32_t EventModelImpl::GetSnoozeCount(const std::string& event_name,
                                        uint32_t window,
                                        uint32_t current_day) const {
  return GetEventCountOrSnooze(event_name, current_day, window,
                               /*is_snooze=*/true);
}

bool EventModelImpl::IsSnoozeDismissed(const std::string& event_name) const {
  const Event* event = GetEvent(event_name);
  return event ? event->snooze_dismissed() : false;
}

void EventModelImpl::OnStoreLoaded(OnModelInitializationFinished callback,
                                   uint32_t current_day,
                                   bool success,
                                   std::unique_ptr<std::vector<Event>> events) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }

  for (auto& event : *events) {
    DCHECK_NE("", event.name());

    Event new_event;
    for (const auto& event_count : event.events()) {
      if (!storage_validator_->ShouldKeep(event.name(), event_count.day(),
                                          current_day)) {
        continue;
      }

      Event_Count* new_event_count = new_event.add_events();
      new_event_count->set_day(event_count.day());
      new_event_count->set_count(event_count.count());
      new_event_count->set_snooze_count(event_count.snooze_count());
    }

    // Only keep Event object that have days with activity.
    if (new_event.events_size() > 0) {
      new_event.set_name(event.name());
      new_event.set_last_snooze_time_us(event.last_snooze_time_us());
      events_[event.name()] = new_event;

      // If the number of events is not the same, overwrite DB entry.
      if (new_event.events_size() != event.events_size())
        store_->WriteEvent(new_event);
    } else {
      // If there are no more activity for an Event, delete the whole event.
      store_->DeleteEvent(event.name());
    }
  }

  ready_ = true;
  std::move(callback).Run(true);
}

int EventModelImpl::GetEventCountOrSnooze(const std::string& event_name,
                                          int current_day,
                                          int window,
                                          bool is_snooze) const {
  const Event* event = GetEvent(event_name);

  // If the Event object is not found, or if the window is 0 days, there will
  // never be any events.
  if (!event || window == 0u)
    return 0;

  DCHECK(window >= 0);

  // A window of N=0:  Nothing should be counted.
  // A window of N=1:  |current_day| should be counted.
  // A window of N=2+: |current_day| plus |N-1| more days should be counted.
  uint32_t oldest_accepted_day = current_day - window + 1;

  // Cap |oldest_accepted_day| to UNIX epoch.
  if (window > current_day)
    oldest_accepted_day = 0u;

  // Calculate the number of events within the window.
  uint32_t count = 0;
  for (const auto& event_day : event->events()) {
    if (event_day.day() < oldest_accepted_day)
      continue;
    count += is_snooze ? event_day.snooze_count() : event_day.count();
  }

  return count;
}

Event& EventModelImpl::GetNonConstEvent(const std::string& event_name) {
  DCHECK_NE("", event_name);
  if (events_.find(event_name) == events_.end()) {
    // Event does not exist yet, so create it.
    events_[event_name].set_name(event_name);
    store_->WriteEvent(events_[event_name]);
  }
  return events_[event_name];
}

}  // namespace feature_engagement
