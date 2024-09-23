// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"

namespace syncer {

class UserEventSyncBridge;

class UserEventServiceImpl : public UserEventService {
 public:
  explicit UserEventServiceImpl(std::unique_ptr<UserEventSyncBridge> bridge);

  UserEventServiceImpl(const UserEventServiceImpl&) = delete;
  UserEventServiceImpl& operator=(const UserEventServiceImpl&) = delete;

  ~UserEventServiceImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  // Checks dynamic or event specific conditions.
  bool ShouldRecordEvent(const sync_pb::UserEventSpecifics& specifics);

  std::unique_ptr<UserEventSyncBridge> bridge_;

  // Holds onto a random number for the duration of this execution of chrome. On
  // restart it will be regenerated. This can be attached to events to know
  // which events came from the same session.
  uint64_t session_id_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SERVICE_IMPL_H_
