// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_FEATURES_H_
#define COMPONENTS_LENS_LENS_FEATURES_H_

#include <string>

#include "base/feature_list.h"

namespace lens {
namespace features {

// Enables context menu search by image sending to the Lens homepage.
extern const base::Feature kLensStandalone;

// Enables Lens Region Search from the context menu.
extern const base::Feature kLensRegionSearch;

// Returns the max pixel width/height for the image to be sent to Lens.
extern int GetMaxPixels();

// The URL for the Lens home page.
extern std::string GetHomepageURL();

}  // namespace features
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
