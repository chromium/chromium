// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

namespace autofill {

class ContactInfoModelTypeController : public syncer::ModelTypeController,
                                       public syncer::SyncServiceObserver {
 public:
  ContactInfoModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service);
  ~ContactInfoModelTypeController() override;

  ContactInfoModelTypeController(const ContactInfoModelTypeController&) =
      delete;
  ContactInfoModelTypeController& operator=(
      const ContactInfoModelTypeController&) = delete;

  // ModelTypeController overrides.
  bool ShouldRunInTransportOnlyMode() const override;
  PreconditionState GetPreconditionState() const override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
