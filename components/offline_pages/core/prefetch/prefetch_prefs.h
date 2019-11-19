// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_

#include <string>

#include "base/time/time.h"

class PrefService;
class PrefRegistrySimple;

namespace offline_pages {

namespace prefetch_prefs {

extern const char kBackoff[];
extern const char kUserSettingEnabled[];

void RegisterPrefs(PrefRegistrySimple* registry);

// Prefetching being enabled depends on three conditions: the feature flag must
// be enabled, the user-controlled setting must be true, and the
// enabled-by-server pref must be true. IsEnabled() checks all of these things
// and is the best indicator of whether prefetching should happen.

// Returns whether the prefetch feature is enabled. Checks the feature
// flag, the user-controlled setting, and the server-enabled state.
bool IsEnabled(PrefService* prefs);

// Configures the user controlled setting that enables or disables the
// prefetching of offline pages to run.
void SetPrefetchingEnabledInSettings(PrefService* prefs, bool enabled);
bool IsPrefetchingEnabledInSettings(PrefService* prefs);

// To be set to |true| when a successful GeneratePageBundle request has been
// made, or |false| if OPS responds with 403 Forbidden. If |enabled| is false,
// the next server-forbidden check will be scheduled to happen after seven days.
void SetEnabledByServer(PrefService* prefs, bool enabled);

bool IsEnabledByServer(PrefService* prefs);

void SetLimitlessPrefetchingEnabled(PrefService* prefs, bool enabled);

bool IsLimitlessPrefetchingEnabled(PrefService* prefs);

// Sets the value of "X-Offline-Prefetch_Testing" to be sent with
// GeneratePageBundle requests. Sending "ForceEnable" instructs OPS not to apply
// its country filtering logic, sending "ForceDisable" causes any
// GeneratePageBundle request to be answered with 403 Forbidden, and omitting
// the header (by passing |value|="") leaves the server to apply its country
// filtering logic as usual.
void SetPrefetchTestingHeader(PrefService* prefs, const std::string& value);

std::string GetPrefetchTestingHeader(PrefService* prefs);

// Returns true if prefetching is enabled and the scheduled GeneratePageBundle
// check time has passed.
bool IsForbiddenCheckDue(PrefService* prefs);

// Returns true if the client has not yet been enabled or disabled by the
// server (implies |IsEnabledByServer()| = false).
bool IsEnabledByServerUnknown(PrefService* prefs);

// Sets "enabled-by-server" to false and sets the forbidden check due time to
// sometime in the past. Note that the prefetching enabled feature flag and
// user-controlled pref must be true in order to force the forbidden check to
// run.
void ResetForbiddenStateForTesting(PrefService* prefs);

// Caches the GCM token across browser restarts so that it can be accessed in
// reduced mode.
void SetCachedPrefetchGCMToken(PrefService* prefs, const std::string& value);
std::string GetCachedPrefetchGCMToken(PrefService* prefs);

}  // namespace prefetch_prefs
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
