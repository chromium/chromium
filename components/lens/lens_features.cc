// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"
#include "base/feature_list.h"

namespace lens {
namespace features {

// Enables context menu search by image sending to lens.google.com.
const base::Feature kLensStandalone{"LensStandalone",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

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
