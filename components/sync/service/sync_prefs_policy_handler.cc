// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_prefs_policy_handler.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

SyncPrefsPolicyHandler::SyncPrefsPolicyHandler(SyncService* sync_service)
    : sync_service_(sync_service) {
  sync_service_->AddObserver(this);
  EnforcePolicyOnDataTypes();
}

SyncPrefsPolicyHandler::~SyncPrefsPolicyHandler() = default;

void SyncPrefsPolicyHandler::OnStateChanged(SyncService* sync_service) {
  EnforcePolicyOnDataTypes();
}

void SyncPrefsPolicyHandler::OnSyncShutdown(SyncService* sync_service) {
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SyncPrefsPolicyHandler::EnforcePolicyOnDataTypes() {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return;
  }

  SyncUserSettings* user_settings = sync_service_->GetUserSettings();
  for (UserSelectableType type :
       user_settings->GetRegisteredSelectableTypes()) {
    switch (user_settings->GetTypePrefStateForAccount(type)) {
      case SyncUserSettings::UserSelectableTypePrefState::kNotApplicable:
      case SyncUserSettings::UserSelectableTypePrefState::kDisabled:
        // To prevent infinite loops of OnStateChanged when SetSelectedType
        // turns the type off, skip disabling the type if it is already
        // disabled.
        continue;
      case SyncUserSettings::UserSelectableTypePrefState::kEnabledOrDefault:
        if (user_settings->IsTypeManagedByPolicy(type) ||
            sync_service_->HasDisableReason(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
          user_settings->SetSelectedType(type, false);
        }
    }
  }
}

}  // namespace syncer
