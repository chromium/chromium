// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace send_tab_to_self {

// Controls syncing of SEND_TAB_TO_SELF.
class SendTabToSelfModelTypeController : public syncer::ModelTypeController,
                                         public syncer::SyncServiceObserver {
 public:
  // The |delegate| and |sync_service| must not be null. Furthermore,
  // |sync_service| must outlive this object.
  SendTabToSelfModelTypeController(
      syncer::SyncService* sync_service,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate);
  ~SendTabToSelfModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  syncer::SyncService* const sync_service_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelTypeController);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_
