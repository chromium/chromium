// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace browser_sync {

// Controls syncing of the AUTOFILL_PROFILE data type.
class AutofillProfileModelTypeController : public syncer::ModelTypeController {
 public:
  AutofillProfileModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      PrefService* pref_service,
      syncer::SyncService* sync_service);
  ~AutofillProfileModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;

  // Registrar for listening to prefs::kAutofillProfileEnabled.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileModelTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_MODEL_TYPE_CONTROLLER_H_
