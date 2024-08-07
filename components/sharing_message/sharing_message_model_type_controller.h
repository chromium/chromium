// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/service/data_type_controller.h"

// Controls syncing of SHARING_MESSAGE.
class SharingMessageModelTypeController : public syncer::DataTypeController {
 public:
  // |delegate_for_full_sync_mode| and |delegate_for_transport_mode| must not be
  // null.
  SharingMessageModelTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode);
  ~SharingMessageModelTypeController() override;
  SharingMessageModelTypeController(const SharingMessageModelTypeController&) =
      delete;
  SharingMessageModelTypeController& operator=(
      const SharingMessageModelTypeController&) = delete;

  // DataTypeController overrides.
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_
