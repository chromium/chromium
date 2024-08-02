// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

// The per-profile service responsible for processing incoming password sharing
// invitations.
class PasswordReceiverService : public KeyedService {
 public:
  PasswordReceiverService() = default;
  PasswordReceiverService(const PasswordReceiverService&) = delete;
  PasswordReceiverService& operator=(const PasswordReceiverService&) = delete;
  ~PasswordReceiverService() override = default;

  virtual void ProcessIncomingSharingInvitation(
      sync_pb::IncomingPasswordSharingInvitationSpecifics invitation) = 0;

  // Used to wire sync data type.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;

  virtual void OnSyncServiceInitialized(syncer::SyncService* service) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_RECEIVER_SERVICE_H_
