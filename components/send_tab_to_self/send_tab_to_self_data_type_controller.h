// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DATA_TYPE_CONTROLLER_H_

#include "components/sync/service/data_type_controller.h"

namespace send_tab_to_self {

// Controls syncing of SEND_TAB_TO_SELF.
class SendTabToSelfDataTypeController : public syncer::DataTypeController {
 public:
  // |delegate_for_full_sync_mode| must not be null.
  // |delegate_for_transport_mode| can be null.
  SendTabToSelfDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode);

  SendTabToSelfDataTypeController(const SendTabToSelfDataTypeController&) =
      delete;
  SendTabToSelfDataTypeController& operator=(
      const SendTabToSelfDataTypeController&) = delete;

  ~SendTabToSelfDataTypeController() override;

  // DataTypeController overrides.
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_DATA_TYPE_CONTROLLER_H_
