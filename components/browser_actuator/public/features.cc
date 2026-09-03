// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/features.h"

namespace browser_actuator {

BASE_FEATURE(kBrowserActuator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kBrowserActuatorInternals, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableBrowserActuatorForGlicExperimentalTriggering,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kBrowserActuatorOAuth2ScopeParam{
    &kBrowserActuator, "BrowserActuatorOAuth2Scope",
    "https://www.googleapis.com/auth/chrome.autobrowse.actuator"};

}  // namespace browser_actuator
