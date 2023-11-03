// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_

#include "components/version_info/channel.h"

class PrefService;
class PrefRegistrySimple;

namespace breadcrumbs {

// A Local State pref that stores whether breadcrumbs is enabled, allowing it to
// stay enabled on the same clients between sessions. This is useful for crashes
// that happen at startup, in case the previous session's log provides insight.
extern const char kEnabledPref[];

// A Local State pref that stores when breadcrumbs was enabled or disabled.
// Breadcrumbs' enabled state is re-randomized after `kEnabledDuration`.
extern const char kEnabledTimePref[];

// Returns whether breadcrumbs logging is enabled. Note that if metrics consent
// was not provided, this will return true but breadcrumbs will not actually be
// uploaded or persisted to disk. If `prefs` is null, breadcrumbs will be
// disabled for the entire session, e.g., for Chrome for Android's minimal mode.
bool IsEnabled(PrefService* prefs);

// Enables or disables breadcrumbs by chance. Returns true if it was enabled,
// and false if it was disabled. Breadcrumbs is enabled at the following rates:
// * 99% on Canary
// * 80% on Dev
// * 80% on Beta
// * 5% on Stable
bool MaybeEnableBasedOnChannel(PrefService* prefs,
                               version_info::Channel channel);

// Registers local-state preferences used by breadcrumbs.
void RegisterPrefs(PrefRegistrySimple* registry);

// Forces `breadcrumbs::IsEnabled()` to return true while it exists. Returns
// breadcrumbs to its default state once destroyed.
class ScopedEnableBreadcrumbsForTesting {
 public:
  ScopedEnableBreadcrumbsForTesting();
  ~ScopedEnableBreadcrumbsForTesting();
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
