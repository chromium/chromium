// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace browser_sync {

// Controls syncing of AUTOFILL_WALLET_DATA and AUTOFILL_WALLET_METADATA.
class AutofillWalletDataTypeController : public syncer::DataTypeController,
                                          public syncer::SyncServiceObserver {
 public:
  // The delegates and |sync_client| must not be null. Furthermore,
  // |sync_client| must outlive this object.
  AutofillWalletDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      PrefService* pref_service,
      syncer::SyncService* sync_service);

  AutofillWalletDataTypeController(const AutofillWalletDataTypeController&) =
      delete;
  AutofillWalletDataTypeController& operator=(
      const AutofillWalletDataTypeController&) = delete;

  ~AutofillWalletDataTypeController() override;

  // DataTypeController overrides.
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  bool IsEnabled() const;
  void SubscribeToPrefChanges();

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<syncer::SyncService> sync_service_;

  PrefChangeRegistrar pref_registrar_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
