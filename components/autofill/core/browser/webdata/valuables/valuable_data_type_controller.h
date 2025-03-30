// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

// A class that manages the startup and shutdown of Autofill Loyalty Cards.
// This custom implementation clears the data on sync pause in kTransportMode.
class AutofillValuableDataTypeController : public syncer::DataTypeController {
 public:
  AutofillValuableDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
          delegate_for_transport_mode);

  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;

 private:
  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_DATA_TYPE_CONTROLLER_H_
