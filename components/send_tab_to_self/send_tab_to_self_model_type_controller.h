// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_

#include "components/sync/service/model_type_controller.h"

namespace send_tab_to_self {

// Controls syncing of SEND_TAB_TO_SELF.
class SendTabToSelfModelTypeController : public syncer::ModelTypeController {
 public:
  // |delegate_for_full_sync_mode| must not be null.
  // |delegate_for_transport_mode| can be null.
  SendTabToSelfModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);

  SendTabToSelfModelTypeController(const SendTabToSelfModelTypeController&) =
      delete;
  SendTabToSelfModelTypeController& operator=(
      const SendTabToSelfModelTypeController&) = delete;

  ~SendTabToSelfModelTypeController() override;

  // DataTypeController overrides.
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_TYPE_CONTROLLER_H_
