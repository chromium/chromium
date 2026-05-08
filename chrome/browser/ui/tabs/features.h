// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_FEATURES_H_

#include "base/feature.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace tabs {

// TODO(crbug.com/346837232): move all flags to this file.

BASE_DECLARE_FEATURE(kTabGroupHome);

// Whether the throbber should be shown for a restored tab after it becomes
// visible, instead of when it's active in the tab strip (this signal is known
// to be broken crbug.com/413080225#comment8).
BASE_DECLARE_FEATURE(kSessionRestoreShowThrobberOnVisible);

// Allows split tabs to be arranged top/bottom.
BASE_DECLARE_FEATURE(kSplitViewHorizontal);
// When enabled, creating a new split tab through the context menu will open a
// submenu to select the split's orientation.
BASE_DECLARE_FEATURE_PARAM(bool, kSplitViewHorizontalDirectAccess);

// Whether or not a split view should restore together.
BASE_DECLARE_FEATURE(kSplitViewTabRestore);

// This feature will be used for the LE rollout of Vertical Tabs. It will have
// an earlier min version than kVerticalTabsLaunch.
BASE_DECLARE_FEATURE(kVerticalTabs);

// This will be used for the full launch of Vertical Tabs with an updated min
// version.
BASE_DECLARE_FEATURE(kVerticalTabsLaunch);
BASE_DECLARE_FEATURE_PARAM(bool, kVerticalTabsToggleInTabContextMenu);

BASE_DECLARE_FEATURE(kVerticalTabsPreviewBadge);

BASE_DECLARE_FEATURE(kVerticalTabsNewBadge);

BASE_DECLARE_FEATURE(kVerticalTabsExpandOnHover);
BASE_DECLARE_FEATURE_PARAM(bool, kVerticalTabsExpandOnHoverDefaultEnabled);

// Default strategy for expand on hover uses a fixed delay before the tab strip
// expands.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kVerticalTabsExpandOnHoverDelay);
// Additional delay after a click inside the tab strip. If this value is 0, no
// click delay is applied.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kVerticalTabsExpandOnHoverClickDelay);

// When enabled, use a velocity heuristic rather than
// `kVerticalTabsExpandOnHoverDelay` and `kVerticalTabsExpandOnHoverClickDelay`
// to determine EOH state.
BASE_DECLARE_FEATURE_PARAM(bool,
                           kVerticalTabsExpandOnHoverUseVelocityHeuristic);
// When using the velocity heuristic, this is the minimum time before EOH can be
// triggered.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kVerticalTabsExpandOnHoverVelocityHeuristicDelay);
// This in the minimum number of samples needed to calculate the heuristic.
BASE_DECLARE_FEATURE_PARAM(
    int,
    kVerticalTabsExpandOnHoverVelocityHeuristicMinSamples);
// The interval with which to sample the mouse position to supplement mouse move
// events.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kVerticalTabsExpandOnHoverVelocityHeuristicInterval);
// Threshold for the ratio of dp/ms of horizontal movement before EOH is
// triggered.
BASE_DECLARE_FEATURE_PARAM(
    double,
    kVerticalTabsExpandOnHoverVelocityHeuristicThreshold);
// Minimum distance from the inside edge of the vertical tab strip before EOH
// can be triggered.
BASE_DECLARE_FEATURE_PARAM(
    int,
    kVerticalTabsExpandOnHoverVelocityHeuristicDistanceFromEdge);
// When distance from edge is set, only evaluate that parameter until this delay
// is reached.
BASE_DECLARE_FEATURE_PARAM(
    base::TimeDelta,
    kVerticalTabsExpandOnHoverVelocityHeuristicEdgeDelay);

BASE_DECLARE_FEATURE(kTabSelectionByPointer);

BASE_DECLARE_FEATURE(kBackToOpener);

BASE_DECLARE_FEATURE(kHorizontalTabStripComboButton);
BASE_DECLARE_FEATURE_PARAM(bool, kHorizontalTabStripComboButtonShowStartOnly);

bool IsVerticalTabsFeatureEnabled();

bool IsVerticalTabsExpandOnHoverFeatureEnabled();

bool IsExpandOnHoverClickDelayEnabled();

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_FEATURES_H_
