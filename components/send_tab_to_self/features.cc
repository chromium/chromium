// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"

namespace send_tab_to_self {

const base::Feature kSendTabToSelfSigninPromo{
    "SendTabToSelfSigninPromo", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const base::Feature kSendTabToSelfV2{"SendTabToSelfV2",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs) {
  // TODO(crbug.com/1015322): SyncPrefs is used directly instead of methods in
  // SyncService due to a dependency of SyncServiceImpl on
  // DeviceInfoSyncService. IsReceivingEnabledByUserOnThisDevice is ultimately
  // used by DeviceInfoSyncClient which is owned by DeviceInfoSyncService.
  syncer::SyncPrefs sync_prefs(prefs);

  // IsFirstSetupComplete() false means sync-the-feature is disabled or the
  // consent is missing (e.g. sync setup in progress). The method can return
  // true without actually checking whether the user is signed-in: if they are
  // not, sync-the-transport won't run and receiving tabs would be impossible
  // anyway.
  if (!sync_prefs.IsFirstSetupComplete())
    return true;

  // Sync-the-feature was fully setup. The user-configurable bits should be
  // respected in this state.
  return sync_prefs.IsSyncRequested() &&
         sync_prefs.GetSelectedTypes().Has(syncer::UserSelectableType::kTabs);
}

}  // namespace send_tab_to_self
