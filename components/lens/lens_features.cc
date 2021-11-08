// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

const base::Feature kLensStandalone{"LensStandalone",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLensRegionSearch{"LensRegionSearch",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kRegionSearchMacCursorFix{
    &kLensRegionSearch, "region-search-mac-cursor-fix", true};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1{
    &kLensRegionSearch, "use-menu-item-alt-text-1", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2{
    &kLensRegionSearch, "use-menu-item-alt-text-2", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText3{
    &kLensRegionSearch, "use-menu-item-alt-text-3", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText4{
    &kLensRegionSearch, "use-menu-item-alt-text-4", true};

const base::FeatureParam<bool> kEnableUKMLoggingForRegionSearch{
    &kLensRegionSearch, "region-search-enable-ukm-logging", false};

const base::FeatureParam<bool> kEnableUKMLoggingForImageSearch{
    &kLensStandalone, "enable-ukm-logging", false};

const base::FeatureParam<bool> kEnableSidePanelForLensRegionSearch{
    &kLensRegionSearch, "region-search-enable-side-panel", true};

const base::FeatureParam<bool> kEnableSidePanelForLensImageSearch{
    &kLensStandalone, "enable-side-panel", false};

constexpr base::FeatureParam<int> kMaxPixelsForRegionSearch{
    &kLensRegionSearch, "region-search-dimensions-max-pixels", 1000};

constexpr base::FeatureParam<int> kMaxAreaForRegionSearch{
    &kLensRegionSearch, "region-search-dimensions-max-area", 1000000};

constexpr base::FeatureParam<int> kMaxPixelsForImageSearch{
    &kLensStandalone, "dimensions-max-pixels", 1000};

constexpr base::FeatureParam<std::string> kHomepageURLForImageSearch{
    &kLensStandalone, "region-search-lens-homepage-url", "https://lens.google.com/"};

constexpr base::FeatureParam<std::string> kHomepageURLForRegionSearch{
    &kLensRegionSearch, "lens-homepage-url", "https://lens.google.com/"};

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

std::string GetHomepageURLForImageSearch() {
  return kHomepageURLForImageSearch.Get();
}

std::string GetHomepageURLForRegionSearch() {
  return kHomepageURLForRegionSearch.Get();
}

}  // namespace features
}  // namespace lens
