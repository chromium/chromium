// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"

namespace feature_engagement {
class EventStorageValidator;
class EventStore;

// A EventModelImpl provides the default implementation of the EventModel.
class EventModelImpl : public EventModel {
 public:
  EventModelImpl(std::unique_ptr<EventStore> store,
                 std::unique_ptr<EventStorageValidator> storage_validator);

  EventModelImpl(const EventModelImpl&) = delete;
  EventModelImpl& operator=(const EventModelImpl&) = delete;

  ~EventModelImpl() override;

  // EventModel implementation.
  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  const Event* GetEvent(const std::string& event_name) const override;
  uint32_t GetEventCount(const std::string& event_name,
                         uint32_t current_day,
                         uint32_t window_size) const override;
  void IncrementEvent(const std::string& event_name,
                      uint32_t current_day) override;
  void ClearEvent(const std::string& event_name) override;
  void IncrementSnooze(const std::string& event_name,
                       uint32_t current_day,
                       base::Time current_time) override;
  void DismissSnooze(const std::string& event_name) override;
  base::Time GetLastSnoozeTimestamp(
      const std::string& event_name) const override;
  uint32_t GetSnoozeCount(const std::string& event_name,
                          uint32_t window,
                          uint32_t current_day) const override;
  bool IsSnoozeDismissed(const std::string& event_name) const override;

 private:
  // Callback for loading the underlying store.
  void OnStoreLoaded(OnModelInitializationFinished callback,
                     uint32_t current_day,
                     bool success,
                     std::unique_ptr<std::vector<Event>> events);

  int GetEventCountOrSnooze(const std::string& event_name,
                            int current_day,
                            int window,
                            bool is_snooze) const;

  // Internal version for getting the non-const version of a stored Event.
  // Creates the event if it is not already stored.
  Event& GetNonConstEvent(const std::string& event_name);

  // The underlying store for all events.
  std::unique_ptr<EventStore> store_;

  // A utility for checking whether new events should be stored and for whether
  // old events should be kept.
  std::unique_ptr<EventStorageValidator> storage_validator_;

  // An in-memory representation of all events.
  std::map<std::string, Event> events_;

  // Whether the model has been fully initialized.
  bool ready_;

  base::WeakPtrFactory<EventModelImpl> weak_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_MODEL_IMPL_H_
