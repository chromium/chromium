// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_model_impl.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

void EventModelImpl::Initialize(const OnModelInitializationFinished& callback,
                                uint32_t current_day) {
  store_->Load(base::Bind(&EventModelImpl::OnStoreLoaded,
                          weak_factory_.GetWeakPtr(), callback, current_day));
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

void EventModelImpl::OnStoreLoaded(
    const OnModelInitializationFinished& callback,
    uint32_t current_day,
    bool success,
    std::unique_ptr<std::vector<Event>> events) {
  if (!success) {
    callback.Run(false);
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
    }

    // Only keep Event object that have days with activity.
    if (new_event.events_size() > 0) {
      new_event.set_name(event.name());
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
  callback.Run(true);
}

Event& EventModelImpl::GetNonConstEvent(const std::string& event_name) {
  if (events_.find(event_name) == events_.end()) {
    // Event does not exist yet, so create it.
    events_[event_name].set_name(event_name);
    store_->WriteEvent(events_[event_name]);
  }
  return events_[event_name];
}

}  // namespace feature_engagement
