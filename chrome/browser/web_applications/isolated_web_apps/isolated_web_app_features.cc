// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"

#include "base/feature_list.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

namespace {
using Availability = policy::DeveloperToolsPolicyHandler::Availability;
}

bool IsIwaDevModeEnabled(Profile* profile) {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(profile)) {
    return false;
  }

  auto availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kDisallowedForForceInstalledExtensions:
    case Availability::kAllowed:
      // If developer tools are allowed or only disabled for force-installed
      // apps, continue.
      break;
  }

  return base::FeatureList::IsEnabled(features::kIsolatedWebAppDevMode);
}

bool IsIwaUnmanagedInstallEnabled(Profile* profile) {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(profile)) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      features::kIsolatedWebAppUnmanagedInstall);
}

}  // namespace web_app
