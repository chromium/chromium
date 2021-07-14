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

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1{
    &kLensRegionSearch, "use-menu-item-alt-text-1", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2{
    &kLensRegionSearch, "use-menu-item-alt-text-2", false};

const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText3{
    &kLensRegionSearch, "use-menu-item-alt-text-3", false};

constexpr base::FeatureParam<int> kMaxPixels{&kLensStandalone,
                                             "dimensions-max-pixels", 1000};

constexpr base::FeatureParam<std::string> kHomepageURL{
    &kLensStandalone, "lens-homepage-url", "https://lens.google.com/"};

int GetMaxPixels() {
  return kMaxPixels.Get();
}

std::string GetHomepageURL() {
  return kHomepageURL.Get();
}

}  // namespace features
}  // namespace lens
