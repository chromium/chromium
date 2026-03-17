// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"

class GoogleGroupsManager;

namespace plus_addresses {

// Shared data type controller for PLUS_ADDRESS and PLUS_ADDRESS_SETTING.
// It is responsible for disabling the types when the feature is not enabled or
// the user type not supported.
// Tested by the sync integration tests.
class PlusAddressDataTypeController : public syncer::DataTypeController {
 public:
  PlusAddressDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      GoogleGroupsManager* google_groups_manager);
  ~PlusAddressDataTypeController() override;

  // DataTypeController:
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

 private:
  const raw_ref<GoogleGroupsManager> google_groups_manager_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_
