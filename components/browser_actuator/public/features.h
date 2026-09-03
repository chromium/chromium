// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_FEATURES_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace browser_actuator {

BASE_DECLARE_FEATURE(kBrowserActuator);
BASE_DECLARE_FEATURE(kBrowserActuatorInternals);
BASE_DECLARE_FEATURE(kEnableBrowserActuatorForGlicExperimentalTriggering);

// The OAuth2 scope used by the Browser Actuator for authentication with
// Google APIs. This is configurable via Finch to support testing.
extern const base::FeatureParam<std::string> kBrowserActuatorOAuth2ScopeParam;

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_FEATURES_H_
