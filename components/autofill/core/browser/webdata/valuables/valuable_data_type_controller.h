// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace autofill {

// A class that manages the startup and shutdown of Valuables. This custom
// implementation clears the data on sync pause in kTransportMode.
class AutofillValuableDataTypeController : public syncer::DataTypeController {
 public:
  AutofillValuableDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
          delegate_for_transport_mode,
      PrefService* pref_service,
      syncer::SyncService* sync_service);

  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;

 private:
  void SubscribeToPrefChanges();
  void OnUserPrefChanged();

  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<syncer::SyncService> sync_service_;

  PrefChangeRegistrar pref_registrar_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_
