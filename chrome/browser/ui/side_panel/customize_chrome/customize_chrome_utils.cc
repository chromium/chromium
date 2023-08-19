// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_utils.h"

#include "base/feature_list.h"
#include "ui/base/ui_base_features.h"

namespace customize_chrome {

bool IsSidePanelEnabled() {
  return base::FeatureList::IsEnabled(features::kCustomizeChromeSidePanel);
}

}  // namespace customize_chrome
