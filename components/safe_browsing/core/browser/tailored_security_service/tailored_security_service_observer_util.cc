// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"

#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "url/gurl.h"

namespace safe_browsing {

bool CanQueryTailoredSecurityForUrl(GURL url) {
  return google_util::IsGoogleDomainUrl(
             url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) ||
         google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

bool CanShowUnconsentedTailoredSecurityDialog(syncer::SyncService* sync_service,
                                              PrefService* prefs) {
  if (IsEnhancedProtectionEnabled(*prefs))
    return false;

  if (!sync_service) {
    return false;
  }

  bool sync_history_enabled =
      sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory);
  if (sync_history_enabled) {
    return false;
  }

  if (prefs->GetBoolean(prefs::kAccountTailoredSecurityShownNotification)) {
    return false;
  }

  if (SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          prefs)) {
    return false;
  }

  return true;
}

}  // namespace safe_browsing
