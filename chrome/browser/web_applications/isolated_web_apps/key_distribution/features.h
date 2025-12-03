// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_FEATURES_H_

#include "base/features.h"

namespace features {

// Enables preventing installation and update of non-allowlisted IWAs.
BASE_DECLARE_FEATURE(kIsolatedWebAppManagedAllowlist);

}  // namespace features

namespace web_app {

// Enables the key distribution dev mode UI on chrome://web-app-internals.
BASE_DECLARE_FEATURE(kIwaKeyDistributionDevMode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_FEATURES_H_
