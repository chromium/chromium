// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/features.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

namespace tabs {

BASE_FEATURE(kTabGroupHome, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSessionRestoreShowThrobberOnVisible,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsLaunch, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsPreviewBadge, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsNewBadge, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSelectionByPointer, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHorizontalTabStripComboButton, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Back-to-Opener behavior, allowing users to press the back button in a
// newly opened tab to close that tab and return focus to the opener tab.
BASE_FEATURE(kBackToOpener, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVerticalTabsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kVerticalTabs) ||
         base::FeatureList::IsEnabled(kVerticalTabsLaunch);
  ;
}

}  // namespace tabs
