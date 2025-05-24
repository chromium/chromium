// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/features.h"

namespace sensitive_content::features {

// When enabled, the content view will be redacted if sensitive fields (such as
// passwords or credit cards) are present on the page. The feature works only on
// Android API level >= 35.
// TODO(crbug.com/343119998): Clean up when launched.
BASE_FEATURE(kSensitiveContent,
             "SensitiveContent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This param controls whether the feature should use password manager
// heuristics to provide more accurate password form predictions, at the cost of
// a potential performance impact.
// TODO(crbug.com/343119998): Clean up when launched.
const base::FeatureParam<bool> kSensitiveContentUsePwmHeuristicsParam{
    &kSensitiveContent, "sensitive_content_use_pwm_heuristics", false};

// When enabled, the tab switching surfaces will be redacted if they offer a
// preview of a tab with sensitive content. The feature works only on Android
// API level >= 35.
// TODO(crbug.com/371547489): Clean up when launched.
BASE_FEATURE(kSensitiveContentWhileSwitchingTabs,
             "SensitiveContentWhileSwitchingTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sensitive_content::features
