// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

const base::Feature kLensStandalone{"LensStandalone",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<bool> kRegionSearchMacCursorFix{
    &kLensStandalone, "region-search-mac-cursor-fix", true};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1{
    &kLensStandalone, "use-menu-item-alt-text-1", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2{
    &kLensStandalone, "use-menu-item-alt-text-2", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText3{
    &kLensStandalone, "use-menu-item-alt-text-3", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText4{
    &kLensStandalone, "use-menu-item-alt-text-4", true};

const base::FeatureParam<bool> kEnableUKMLoggingForRegionSearch{
    &kLensStandalone, "region-search-enable-ukm-logging", true};

const base::FeatureParam<bool> kEnableUKMLoggingForImageSearch{
    &kLensStandalone, "enable-ukm-logging", true};

const base::FeatureParam<bool> kEnableSidePanelForLens{
    &kLensStandalone, "enable-side-panel", true};

constexpr base::FeatureParam<int> kMaxPixelsForRegionSearch{
    &kLensStandalone, "region-search-dimensions-max-pixels", 1000};

constexpr base::FeatureParam<int> kMaxAreaForRegionSearch{
    &kLensStandalone, "region-search-dimensions-max-area", 1000000};

constexpr base::FeatureParam<int> kMaxPixelsForImageSearch{
    &kLensStandalone, "dimensions-max-pixels", 1000};

constexpr base::FeatureParam<std::string> kHomepageURLForLens{
    &kLensStandalone, "lens-homepage-url", "https://lens.google.com/"};

bool GetEnableUKMLoggingForRegionSearch() {
  return kEnableUKMLoggingForRegionSearch.Get();
}

bool GetEnableUKMLoggingForImageSearch() {
  return kEnableUKMLoggingForImageSearch.Get();
}

int GetMaxPixelsForRegionSearch() {
  return kMaxPixelsForRegionSearch.Get();
}

int GetMaxAreaForRegionSearch() {
  return kMaxAreaForRegionSearch.Get();
}

int GetMaxPixelsForImageSearch() {
  return kMaxPixelsForImageSearch.Get();
}

std::string GetHomepageURLForLens() {
  return kHomepageURLForLens.Get();
}

bool IsLensSidePanelEnabled() {
  return base::FeatureList::IsEnabled(kLensStandalone) &&
         kEnableSidePanelForLens.Get();
}

}  // namespace features
}  // namespace lens
