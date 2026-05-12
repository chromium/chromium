// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"

namespace tabs {

BASE_FEATURE(kTabGroupHome, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSessionRestoreShowThrobberOnVisible,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSplitViewHorizontal, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kSplitViewHorizontalDirectAccess,
                   &kSplitViewHorizontal,
                   "split_view_horizontal_direct_access",
                   false);

BASE_FEATURE(kSplitViewTabRestore, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsLaunch, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kVerticalTabsToggleInTabContextMenu,
                   &kVerticalTabsLaunch,
                   "toggle_in_tab_context_menu",
                   true);

BASE_FEATURE(kVerticalTabsPreviewBadge, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsNewBadge, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalTabsExpandOnHover, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kVerticalTabsExpandOnHoverDefaultEnabled,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_default_enabled",
                   false);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kVerticalTabsExpandOnHoverDelay,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_delay",
                   base::Milliseconds(350));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVerticalTabsExpandOnHoverClickDelay,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_click_delay",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(bool,
                   kVerticalTabsExpandOnHoverUseVelocityHeuristic,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_use_velocity_heuristic",
                   false);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVerticalTabsExpandOnHoverVelocityHeuristicDelay,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_delay",
                   base::Milliseconds(0));
BASE_FEATURE_PARAM(int,
                   kVerticalTabsExpandOnHoverVelocityHeuristicMinSamples,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_min_samples",
                   2);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVerticalTabsExpandOnHoverVelocityHeuristicInterval,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_interval",
                   base::Milliseconds(10));
BASE_FEATURE_PARAM(double,
                   kVerticalTabsExpandOnHoverVelocityHeuristicThreshold,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_threshold",
                   0.1);
BASE_FEATURE_PARAM(int,
                   kVerticalTabsExpandOnHoverVelocityHeuristicDistanceFromEdge,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_distance_from_edge",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVerticalTabsExpandOnHoverVelocityHeuristicEdgeDelay,
                   &kVerticalTabsExpandOnHover,
                   "expand_on_hover_velocity_heuristic_edge_delay",
                   base::Milliseconds(0));

BASE_FEATURE(kTabSelectionByPointer, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHorizontalTabStripComboButton, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kHorizontalTabStripComboButtonShowStartOnly,
                   &kHorizontalTabStripComboButton,
                   "show_start_only",
                   false);

// Enables Back-to-Opener behavior, allowing users to press the back button in a
// newly opened tab to close that tab and return focus to the opener tab.
BASE_FEATURE(kBackToOpener, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVerticalTabsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kVerticalTabs) ||
         base::FeatureList::IsEnabled(kVerticalTabsLaunch);
  ;
}

bool IsVerticalTabsExpandOnHoverFeatureEnabled() {
  return IsVerticalTabsFeatureEnabled() &&
         base::FeatureList::IsEnabled(kVerticalTabsExpandOnHover);
}

bool IsExpandOnHoverClickDelayEnabled() {
  return !kVerticalTabsExpandOnHoverClickDelay.Get().is_zero();
}

}  // namespace tabs
