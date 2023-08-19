// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_UTIL_H_

#include "base/time/time.h"

class GURL;
class PrefService;

namespace syncer {
class SyncService;
}

namespace safe_browsing {

inline constexpr base::TimeDelta kThresholdForInFlowNotification =
    base::Minutes(5);

// Checks if we can query TailoredSecurity for a url.
bool CanQueryTailoredSecurityForUrl(GURL url);

// Checks if we can show the unconsented tailored security dialog depending on
// the user's identity and preferences.
bool CanShowUnconsentedTailoredSecurityDialog(syncer::SyncService* sync_service,
                                              PrefService* prefs);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_OBSERVER_UTIL_H_
