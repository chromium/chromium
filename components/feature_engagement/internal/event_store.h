// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORE_H_

#include <string>

#include "base/functional/callback.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"

namespace feature_engagement {

// EventStore represents the storage engine behind the EventModel.
class EventStore {
 public:
  using OnLoadedCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::vector<Event>>)>;

  EventStore(const EventStore&) = delete;
  EventStore& operator=(const EventStore&) = delete;

  virtual ~EventStore() = default;

  // Loads the database from storage and asynchronously posts the result back
  // on the caller's thread.
  // Ownership of the loaded data is given to the caller.
  virtual void Load(OnLoadedCallback callback) = 0;

  // Returns whether the database is ready, i.e. whether it has been fully
  // loaded.
  virtual bool IsReady() const = 0;

  // Stores the given event to persistent storage.
  virtual void WriteEvent(const Event& event) = 0;

  // Deletes the event with the given name.
  virtual void DeleteEvent(const std::string& event_name) = 0;

 protected:
  EventStore() = default;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORE_H_
