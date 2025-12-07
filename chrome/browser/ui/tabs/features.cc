// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/features.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

namespace tabs {

// Enables the debug UI used to visualize the tab strip model.
// chrome://tab-strip-internals
BASE_FEATURE(kDebugUITabStrip, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupHome, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSearchPositionSetting, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSelectionByPointer, base::FEATURE_DISABLED_BY_DEFAULT);

bool CanShowTabSearchPositionSetting() {
  // Alternate tab search locations cannot be repositioned.
  if (features::HasTabSearchToolbarButton()) {
    return false;
  }
// Mac and other platforms will always have the tab search position in the
// correct location, cros/linux/win git the user the option to change.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(kTabSearchPositionSetting);
#else
  return false;
#endif
}

bool IsVerticalTabsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kVerticalTabs);
}

}  // namespace tabs
