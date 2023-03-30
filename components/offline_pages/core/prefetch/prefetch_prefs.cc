// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace offline_pages {
namespace prefetch_prefs {
namespace {
// Prefs only accessed in this file
const char kLimitlessPrefetchingEnabledTimePref[] =
    "offline_prefetch.limitless_prefetching_enabled_time";
const char kPrefetchTestingHeaderPref[] =
    "offline_prefetch.testing_header_value";
const char kEnabledByServer[] = "offline_prefetch.enabled_by_server";
const char kNextForbiddenCheckTimePref[] = "offline_prefetch.next_gpb_check";
const char kPrefetchCachedGCMToken[] = "offline_prefetch.gcm_token";

}  // namespace

const char kUserSettingEnabled[] = "offline_prefetch.enabled";
const char kBackoff[] = "offline_prefetch.backoff";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kBackoff);
  registry->RegisterBooleanPref(kUserSettingEnabled, true);
  registry->RegisterTimePref(kLimitlessPrefetchingEnabledTimePref,
                             base::Time());
  registry->RegisterStringPref(kPrefetchTestingHeaderPref, std::string());
  registry->RegisterBooleanPref(kEnabledByServer, false);
  registry->RegisterTimePref(kNextForbiddenCheckTimePref, base::Time());
  registry->RegisterStringPref(kPrefetchCachedGCMToken, std::string());
}

}  // namespace prefetch_prefs
}  // namespace offline_pages
