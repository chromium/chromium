// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service.h"

namespace signin {
class IdentityManager;
}

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace autofill {

class ContactInfoModelTypeController : public syncer::ModelTypeController {
 public:
  ContactInfoModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager);
  ~ContactInfoModelTypeController() override;

  ContactInfoModelTypeController(const ContactInfoModelTypeController&) =
      delete;
  ContactInfoModelTypeController& operator=(
      const ContactInfoModelTypeController&) = delete;

  // ModelTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  PreconditionState GetPreconditionState() const override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;

 private:
  ContactInfoPreconditionChecker precondition_checker_;
  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
