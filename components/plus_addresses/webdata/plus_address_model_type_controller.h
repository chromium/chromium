// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_

#include "components/sync/service/model_type_controller.h"

namespace plus_addresses {

class PlusAddressModelTypeController : public syncer::ModelTypeController {
 public:
  PlusAddressModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);
  ~PlusAddressModelTypeController() override;

  // ModelTypeController:
  PreconditionState GetPreconditionState() const override;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_MODEL_TYPE_CONTROLLER_H_
