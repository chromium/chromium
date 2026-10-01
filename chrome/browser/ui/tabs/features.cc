// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/features.h"

#include "base/feature.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"

namespace tabs {

BASE_FEATURE(kTabGroupHome, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSessionRestoreShowThrobberOnVisible,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitViewHorizontal, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kSplitViewHorizontalDirectAccess,
                   &kSplitViewHorizontal,
                   "split_view_horizontal_direct_access",
                   false);
BASE_FEATURE_PARAM(bool,
                   kSplitViewHorizontalDirectTabAccess,
                   &kSplitViewHorizontal,
                   "split_view_horizontal_direct_tab_access",
                   false);

BASE_FEATURE(kTabSearchCjkWordBoundary, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabSearchPerformanceImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kVerticalTabsNewBadge, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripUnification, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewHorizontalPinnedTabStyling, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Back-to-Opener behavior, allowing users to press the back button in a
// newly opened tab to close that tab and return focus to the opener tab.
BASE_FEATURE(kBackToOpener, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSplitViewHorizontalIndirectAccessEnabled() {
  return base::FeatureList::IsEnabled(kSplitViewHorizontal) &&
         !kSplitViewHorizontalDirectAccess.Get();
}

bool IsSplitViewHorizontalDirectAccessEnabledForTab() {
  return base::FeatureList::IsEnabled(kSplitViewHorizontal) &&
         (kSplitViewHorizontalDirectAccess.Get() ||
          kSplitViewHorizontalDirectTabAccess.Get());
}

bool IsNewHorizontalPinnedTabStylingEnabled() {
  return base::FeatureList::IsEnabled(kTabStripUnification) &&
         base::FeatureList::IsEnabled(kNewHorizontalPinnedTabStyling);
}

}  // namespace tabs
