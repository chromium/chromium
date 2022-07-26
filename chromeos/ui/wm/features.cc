// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/features.h"

namespace chromeos {
namespace wm {
namespace features {

// Enables a window to float.
// https://crbug.com/1240411
const base::Feature kFloatWindow{"CrOSLabsFloatWindow",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

bool IsFloatWindowEnabled() {
  return base::FeatureList::IsEnabled(kFloatWindow);
}

}  // namespace features
}  // namespace wm
}  // namespace chromeos
