// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_IN_MEMORY_EVENT_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_IN_MEMORY_EVENT_STORE_H_

#include <vector>

#include "components/feature_engagement/internal/event_store.h"

namespace feature_engagement {
// An InMemoryEventStore provides a DB layer that stores all data in-memory.
// All data is made available to this class during construction, and can be
// loaded once by a caller. All calls to WriteEvent(...) are ignored.
class InMemoryEventStore : public EventStore {
 public:
  explicit InMemoryEventStore(std::unique_ptr<std::vector<Event>> events);
  InMemoryEventStore();

  InMemoryEventStore(const InMemoryEventStore&) = delete;
  InMemoryEventStore& operator=(const InMemoryEventStore&) = delete;

  ~InMemoryEventStore() override;

  // EventStore implementation.
  void Load(OnLoadedCallback callback) override;
  bool IsReady() const override;
  void WriteEvent(const Event& event) override;
  void DeleteEvent(const std::string& event_name) override;

 protected:
  // Posts the result of loading and sets up the ready state.
  // Protected and virtual for testing.
  virtual void HandleLoadResult(OnLoadedCallback callback, bool success);

 private:
  // All events that this in-memory store was constructed with. This will be
  // reset when Load(...) is called.
  std::unique_ptr<std::vector<Event>> events_;

  // Whether the store is ready or not. It is true after Load(...) has been
  // invoked.
  bool ready_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_IN_MEMORY_EVENT_STORE_H_
