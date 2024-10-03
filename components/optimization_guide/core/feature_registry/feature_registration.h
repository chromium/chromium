// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_

#include "base/component_export.h"
#include "base/feature_list.h"

class PrefRegistrySimple;
namespace optimization_guide {

namespace prefs {
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kTabOrganizationEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kComposeEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kWallpaperSearchEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHistorySearchEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kProductSpecificationsEnterprisePolicyAllowed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFormsPredictionsEnterprisePolicyAllowed[];
}  // namespace prefs

namespace features {
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kComposeMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kTabOrganizationMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kWallpaperSearchMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kHistorySearchMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kProductSpecificationsMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kFormsPredictionsMqlsLogging);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
BASE_DECLARE_FEATURE(kFormsAnnotationsMqlsLogging);
}  // namespace features

void RegisterGenAiFeatures(PrefRegistrySimple* registry);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_FEATURE_REGISTRATION_H_
