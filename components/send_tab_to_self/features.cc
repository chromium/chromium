// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"

namespace send_tab_to_self {

const base::Feature kSendTabToSelfWhenSignedIn{
    "SendTabToSelfWhenSignedIn", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSendTabToSelfUseFakeBackend{
    "SendTabToSelfUseFakeBackend", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs) {
  // TODO(crbug.com/1015322): SyncPrefs is used directly instead of methods in
  // SyncService due to a dependency of ProfileSyncService on
  // DeviceInfoSyncService. IsReceivingEnabledByUserOnThisDevice is ultimately
  // used by DeviceInfoSyncClient which is owned by DeviceInfoSyncService.
  syncer::SyncPrefs sync_prefs(prefs);

  if (sync_prefs.IsFirstSetupComplete()) {
    // Sync-the-feature was fully setup. Regardless of
    // kSendTabToSelfWhenSignedIn, the user-configurable bits should be
    // respected in this state.
    return sync_prefs.IsSyncRequested() &&
           sync_prefs.GetSelectedTypes().Has(syncer::UserSelectableType::kTabs);
  }

  // Sync-the-feature is disabled or the consent is missing (e.g. sync setup in
  // progress). If kSendTabToSelfWhenSignedIn is disabled, receiving shouldn't
  // be allowed in this state. If kSendTabToSelfWhenSignedIn is enabled, the
  // method can return true without actually checking whether the user is
  // signed-in: if they are not, sync-the-transport won't run and receiving tabs
  // would be impossible anyway.
  return base::FeatureList::IsEnabled(kSendTabToSelfWhenSignedIn);
}

}  // namespace send_tab_to_self
