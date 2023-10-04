// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace password_manager {
class PasswordReceiverService;

class IncomingPasswordSharingInvitationModelTypeController
    : public syncer::ModelTypeController,
      public syncer::SyncServiceObserver {
 public:
  IncomingPasswordSharingInvitationModelTypeController(
      syncer::SyncService* sync_service,
      PasswordReceiverService* password_receiver_service);

  IncomingPasswordSharingInvitationModelTypeController(
      const IncomingPasswordSharingInvitationModelTypeController&) = delete;
  IncomingPasswordSharingInvitationModelTypeController& operator=(
      const IncomingPasswordSharingInvitationModelTypeController&) = delete;
  ~IncomingPasswordSharingInvitationModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  const raw_ptr<syncer::SyncService> sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_INCOMING_PASSWORD_SHARING_INVITATION_MODEL_TYPE_CONTROLLER_H_
