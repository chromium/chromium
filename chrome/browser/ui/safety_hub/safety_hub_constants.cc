// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "base/time/time.h"

namespace safety_hub {

const char kCardHeaderKey[] = "header";
const char kCardSubheaderKey[] = "subheader";
const char kCardStateKey[] = "state";
const char kSafetyHubSafeBrowsingStatusKey[] = "safeBrowsingStatus";

const char kSafetyHubMenuNotificationActiveKey[] = "isCurrentlyActive";
const char kSafetyHubMenuNotificationAllTimeCountKey[] = "allTimeCount";
const char kSafetyHubMenuNotificationImpressionCountKey[] = "impressionCount";
const char kSafetyHubMenuNotificationFirstImpressionKey[] =
    "firstImpressionTime";
const char kSafetyHubMenuNotificationLastImpressionKey[] = "lastImpressionTime";
const char kSafetyHubMenuNotificationShowAfterTimeKey[] = "onlyShowAfterTime";
const char kSafetyHubMenuNotificationResultKey[] = "result";

const char kSafetyHubTriggeringExtensionIdsKey[] = "triggeringExtensions";

const char kExpirationKey[] = "expiration";
const char kLifetimeKey[] = "lifetime";
const char kSafetyHubChooserPermissionsData[] = "chooserPermissionsData";
const char kAbusiveRevocationExpirationKey[] = "abusiveRevocationExpiration";
const char kAbusiveRevocationLifetimeKey[] = "abusiveRevocationLifetime";

const char kRevokedStatusDictKeyStr[] = "revoked_status";
const char kIgnoreStr[] = "ignore";
const char kRevokeStr[] = "revoke";

const char kOrigin[] = "origin";
const char kUsername[] = "username";
const char kSafetyHubPasswordCheckOriginsKey[] = "passwordCheckOrigins";

#if BUILDFLAG(IS_ANDROID)
const char kSafetyHubCompromiedPasswordOriginsCount[] =
    "passwordCheckCompromisedOriginsNum";
#endif  // BUILDFLAG(IS_ANDROID)

const char kBlocklistCheckCountHistogramName[] =
    "Settings.SafetyHub.AbusiveNotificationPermissionRevocation."
    "BlocklistCheckCount";

const base::TimeDelta kMinTimeBetweenPasswordChecks = base::Hours(1);

const base::TimeDelta kRevocationCleanUpThresholdWithDelayForTesting =
    base::Minutes(30);

}  // namespace safety_hub
