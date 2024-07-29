// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/model_type_controller.h"

class GoogleGroupsManager;

namespace plus_addresses {

// Shared model type controller for PLUS_ADDRESS and PLUS_ADDRESS_SETTING.
// It is responsible for disabling the types when the feature is not enabled or
// the user type not supported.
class PlusAddressModelTypeController : public syncer::ModelTypeController {
 public:
  PlusAddressModelTypeController(
      syncer::ModelType type,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      GoogleGroupsManager* google_groups_manager);

  // ModelTypeController:
  PreconditionState GetPreconditionState() const override;

 private:
  const raw_ref<GoogleGroupsManager> google_groups_manager_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_
