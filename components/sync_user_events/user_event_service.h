// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace syncer {

class DataTypeControllerDelegate;

class UserEventService : public KeyedService {
 public:
  UserEventService() = default;

  UserEventService(const UserEventService&) = delete;
  UserEventService& operator=(const UserEventService&) = delete;

  ~UserEventService() override = default;

  // Records a given event to be reported. Relevant settings will be checked to
  // verify user events should be emitted and this will no-op if the the
  // requisite permissions are not present.
  virtual void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) = 0;
  virtual void RecordUserEvent(
      const sync_pb::UserEventSpecifics& specifics) = 0;

  // Returns the underlying Sync integration point.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_H_
