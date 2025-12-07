// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_
#define COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace sensitive_content::features {

COMPONENT_EXPORT(SENSITIVE_CONTENT_FEATURES)
BASE_DECLARE_FEATURE(kSensitiveContent);

COMPONENT_EXPORT(SENSITIVE_CONTENT_FEATURES)
extern const base::FeatureParam<bool> kSensitiveContentUsePwmHeuristicsParam;

COMPONENT_EXPORT(SENSITIVE_CONTENT_FEATURES)
BASE_DECLARE_FEATURE(kSensitiveContentWhileSwitchingTabs);

}  // namespace sensitive_content::features

#endif  // COMPONENTS_SENSITIVE_CONTENT_FEATURES_H_
