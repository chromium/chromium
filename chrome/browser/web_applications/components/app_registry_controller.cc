// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/app_registry_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

AppRegistryController::AppRegistryController(Profile* profile)
    : profile_(profile) {}

AppRegistryController::~AppRegistryController() = default;

void AppRegistryController::SetExperimentalTabbedWindowMode(
    const AppId& app_id,
    bool enabled,
    bool is_user_action) {
  if (enabled) {
    DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip));
    UpdateBoolWebAppPref(profile()->GetPrefs(), app_id,
                         kExperimentalTabbedWindowMode, true);

    // Set non-experimental window mode to standalone for when the user disables
    // this flag.
    SetAppUserDisplayMode(app_id, DisplayMode::kStandalone, is_user_action);
  } else {
    RemoveWebAppPref(profile()->GetPrefs(), app_id,
                     kExperimentalTabbedWindowMode);
  }
}

}  // namespace web_app
