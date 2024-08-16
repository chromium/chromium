// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_
#define COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_

#include <memory>

#include "components/sync_user_events/user_event_service.h"

namespace syncer {

// This implementation is used when we know event should never be recorded,
// such as in incognito mode.
class NoOpUserEventService : public UserEventService {
 public:
  NoOpUserEventService();

  NoOpUserEventService(const NoOpUserEventService&) = delete;
  NoOpUserEventService& operator=(const NoOpUserEventService&) = delete;

  ~NoOpUserEventService() override;

  // UserEventService implementation.
  void RecordUserEvent(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics) override;
  void RecordUserEvent(const sync_pb::UserEventSpecifics& specifics) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_NO_OP_USER_EVENT_SERVICE_H_
