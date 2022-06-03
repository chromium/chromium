// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/features.h"

namespace chromeos {
namespace wm {
namespace features {

// Enables vertical snap for clamshell mode. This allows users to snap top and
// bottom when the screen is in portrait orientation, while snap left and right
// when the screen is in landscape orientation.
const base::Feature kVerticalSnap{"VerticalSnap",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

bool IsVerticalSnapEnabled() {
  return base::FeatureList::IsEnabled(kVerticalSnap);
}

}  // namespace features
}  // namespace wm
}  // namespace chromeos
