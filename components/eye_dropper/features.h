// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// limited to top chrome UI.

#ifndef COMPONENTS_EYE_DROPPER_FEATURES_H_
#define COMPONENTS_EYE_DROPPER_FEATURES_H_

#include "base/feature_list.h"

namespace eye_dropper::features {

BASE_DECLARE_FEATURE(kAllowEyeDropperWGCScreenCapture);

}  // namespace eye_dropper::features

#endif  // COMPONENTS_EYE_DROPPER_FEATURES_H_
