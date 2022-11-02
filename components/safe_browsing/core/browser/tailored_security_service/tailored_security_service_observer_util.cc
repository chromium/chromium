// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace safe_browsing {

bool CanQueryTailoredSecurityForUrl(GURL url) {
  return url.DomainIs("google.com") || url.DomainIs("youtube.com");
}

bool CanShowUnconsentedTailoredSecurityDialog(
    signin::IdentityManager* identity_manager,
    PrefService* prefs) {
  if (IsEnhancedProtectionEnabled(*prefs))
    return false;

  if (!identity_manager ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
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
