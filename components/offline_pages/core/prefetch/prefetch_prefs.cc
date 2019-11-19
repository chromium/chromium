// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
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
const base::TimeDelta kForbiddenCheckDelay = base::TimeDelta::FromDays(7);
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

void SetPrefetchingEnabledInSettings(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kUserSettingEnabled, enabled);
}

bool IsPrefetchingEnabledInSettings(PrefService* prefs) {
  return prefs->GetBoolean(kUserSettingEnabled);
}

bool IsEnabled(PrefService* prefs) {
  return IsPrefetchingOfflinePagesEnabled() &&
         prefs->GetBoolean(kUserSettingEnabled) && IsEnabledByServer(prefs);
}

void SetLimitlessPrefetchingEnabled(PrefService* prefs, bool enabled) {
  DCHECK(prefs);
  if (enabled)
    prefs->SetTime(kLimitlessPrefetchingEnabledTimePref, OfflineTimeNow());
  else
    prefs->SetTime(kLimitlessPrefetchingEnabledTimePref, base::Time());
}

bool IsLimitlessPrefetchingEnabled(PrefService* prefs) {
  base::TimeDelta max_duration;
  if (version_info::IsOfficialBuild())
    max_duration = base::TimeDelta::FromDays(1);
  else
    max_duration = base::TimeDelta::FromDays(365);

  DCHECK(prefs);
  const base::Time enabled_time =
      prefs->GetTime(kLimitlessPrefetchingEnabledTimePref);
  const base::Time now = OfflineTimeNow();

  return (now >= enabled_time) && (now < (enabled_time + max_duration));
}

void SetPrefetchTestingHeader(PrefService* prefs, const std::string& value) {
  DCHECK(prefs);
  prefs->SetString(kPrefetchTestingHeaderPref, value);
}

std::string GetPrefetchTestingHeader(PrefService* prefs) {
  DCHECK(prefs);
  return prefs->GetString(kPrefetchTestingHeaderPref);
}

bool IsForbiddenCheckDue(PrefService* prefs) {
  DCHECK(prefs);
  base::Time checkTime = prefs->GetTime(kNextForbiddenCheckTimePref);
  return IsPrefetchingOfflinePagesEnabled() &&
         prefs->GetBoolean(kUserSettingEnabled) && !IsEnabledByServer(prefs) &&
         (checkTime < OfflineTimeNow() ||  // did the delay expire?
          checkTime >
              OfflineTimeNow() +
                  kForbiddenCheckDelay);  // is the next time unreasonably far
                                          // in the future (e.g. clock change)?
}

bool IsEnabledByServerUnknown(PrefService* prefs) {
  DCHECK(prefs);
  return IsForbiddenCheckDue(prefs) &&
         (prefs->GetTime(kNextForbiddenCheckTimePref) == base::Time());
}

void SetEnabledByServer(PrefService* prefs, bool enabled) {
  DCHECK(prefs);
  prefs->SetBoolean(kEnabledByServer, enabled);
  if (!enabled) {
    prefs->SetTime(kNextForbiddenCheckTimePref,
                   OfflineTimeNow() + kForbiddenCheckDelay);
  }
}

bool IsEnabledByServer(PrefService* prefs) {
  DCHECK(prefs);
  return prefs->GetBoolean(kEnabledByServer);
}

void ResetForbiddenStateForTesting(PrefService* prefs) {
  DCHECK(prefs);
  SetEnabledByServer(prefs, false);
  prefs->SetTime(kNextForbiddenCheckTimePref, base::Time());
}

void SetCachedPrefetchGCMToken(PrefService* prefs, const std::string& value) {
  DCHECK(prefs);
  prefs->SetString(kPrefetchCachedGCMToken, value);
}

std::string GetCachedPrefetchGCMToken(PrefService* prefs) {
  DCHECK(prefs);
  return prefs->GetString(kPrefetchCachedGCMToken);
}

}  // namespace prefetch_prefs
}  // namespace offline_pages
