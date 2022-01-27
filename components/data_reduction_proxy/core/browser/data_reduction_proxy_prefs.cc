// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include <memory>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_reduction_proxy {

// Make sure any changes here that have the potential to impact android_webview
// are reflected in RegisterSimpleProfilePrefs.
void RegisterSyncableProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  registry->RegisterBooleanPref(prefs::kDataReductionProxyWasEnabledBefore,
                                false);

  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastEnabledTime, 0L);
}

void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyWasEnabledBefore, false);

  RegisterPrefs(registry);
}

// Add any new data reduction proxy prefs to the |pref_map_| or the
// |list_pref_map_| in Init() of DataReductionProxyCompressionStats.
void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastEnabledTime, 0L);
}

}  // namespace data_reduction_proxy
