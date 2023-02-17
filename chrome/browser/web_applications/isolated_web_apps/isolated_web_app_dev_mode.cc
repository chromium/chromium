// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"

namespace web_app {

bool IsIwaDevModeEnabled(const PrefService& pref_service) {
  if (!base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return false;
  }

  if (!pref_service.GetBoolean(
          policy::policy_prefs::kIsolatedAppsDeveloperModeAllowed)) {
    return false;
  }

  return base::FeatureList::IsEnabled(features::kIsolatedWebAppDevMode);
}

}  // namespace web_app
