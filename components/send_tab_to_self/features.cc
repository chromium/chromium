// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"

namespace send_tab_to_self {

const base::Feature kSendTabToSelfOmniboxSendingAnimation{
    "SendTabToSelfOmniboxSendingAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendTabToSelfWhenSignedIn{
    "SendTabToSelfWhenSignedIn", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs) {
  // TODO(crbug.com/1015322): SyncPrefs is used directly instead of methods in
  // SyncService due to a dependency of ProfileSyncService on
  // DeviceInfoSyncService. IsReceivingEnabledByUserOnThisDevice is ultimately
  // used by DeviceInfoSyncClient which is owend by DeviceInfoSyncService.
  syncer::SyncPrefs sync_prefs(prefs);
  // As per documentation in SyncUserSettings, IsSyncRequested indicates user
  // wants Sync to run, when combined with IsFirstSetupComplete, indicates
  // whether user has consented to Sync.
  return sync_prefs.IsSyncRequested() && sync_prefs.IsFirstSetupComplete() &&
         sync_prefs.GetSelectedTypes().Has(syncer::UserSelectableType::kTabs);
}
}  // namespace send_tab_to_self
