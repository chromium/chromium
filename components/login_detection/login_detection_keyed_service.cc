// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login_detection/login_detection_keyed_service.h"

#include "base/check.h"
#include "components/login_detection/login_detection_prefs.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace login_detection {

LoginDetectionKeyedService::LoginDetectionKeyedService(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);
  // Apply site isolation to logged-in sites that had previously been saved by
  // login detection. Needs to be called before any navigations happen in
  // `browser_context`.
  //
  // TODO(alexmos): Move this initialization to components/site_isolation once
  // login detection is moved into its own component.
  PrefService* pref_service = user_prefs::UserPrefs::Get(browser_context);
  CHECK(pref_service);
  site_isolation::SiteIsolationPolicy::IsolateStoredOAuthSites(
      browser_context, prefs::GetOAuthSignedInSites(pref_service));
}

LoginDetectionKeyedService::~LoginDetectionKeyedService() = default;

}  // namespace login_detection
