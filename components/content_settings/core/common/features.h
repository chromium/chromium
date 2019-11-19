// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_

#include "base/component_export.h"
#include "base/metrics/field_trial_params.h"

namespace base {
struct Feature;
}  // namespace base

namespace content_settings {

// Feature to enable a better cookie controls ui.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::Feature kImprovedCookieControls;

COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::FeatureParam<bool> kImprovedCookieControlsDefaultInIncognito;

// Feature to enable the improved cookie contronls ui for third-party cookie
// blocking users.
COMPONENT_EXPORT(CONTENT_SETTINGS_FEATURES)
extern const base::Feature kImprovedCookieControlsForThirdPartyCookieBlocking;

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_FEATURES_H_