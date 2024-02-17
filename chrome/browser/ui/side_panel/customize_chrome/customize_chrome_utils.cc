// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "ui/base/ui_base_features.h"

namespace customize_chrome {

bool IsSidePanelEnabled() {
  return base::FeatureList::IsEnabled(features::kCustomizeChromeSidePanel);
}

bool IsWallpaperSearchEnabledForProfile(Profile* profile) {
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return base::FeatureList::IsEnabled(
             ntp_features::kCustomizeChromeWallpaperSearch) &&
         base::FeatureList::IsEnabled(
             optimization_guide::features::kOptimizationGuideModelExecution) &&
         features::IsChromeWebuiRefresh2023() &&
         (optimization_guide_keyed_service &&
          optimization_guide_keyed_service
              ->ShouldFeatureBeCurrentlyEnabledForUser(
                  optimization_guide::proto::ModelExecutionFeature::
                      MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH));
}

}  // namespace customize_chrome
