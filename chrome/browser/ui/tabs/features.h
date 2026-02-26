// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace tabs {

// TODO(crbug.com/346837232): move all flags to this file.

BASE_DECLARE_FEATURE(kTabGroupHome);

// Whether the throbber should be shown for a restored tab after it becomes
// visible, instead of when it's active in the tab strip (this signal is known
// to be broken crbug.com/413080225#comment8).
BASE_DECLARE_FEATURE(kSessionRestoreShowThrobberOnVisible);

// This feature will be used for the LE rollout of Vertical Tabs. It will have
// an earlier min version than kVerticalTabsLaunch.
BASE_DECLARE_FEATURE(kVerticalTabs);

// This will be used for the full launch of Vertical Tabs with an updated min
// version.
BASE_DECLARE_FEATURE(kVerticalTabsLaunch);

BASE_DECLARE_FEATURE(kVerticalTabsPreviewBadge);

BASE_DECLARE_FEATURE(kVerticalTabsNewBadge);

BASE_DECLARE_FEATURE(kTabSelectionByPointer);

BASE_DECLARE_FEATURE(kBackToOpener);

BASE_DECLARE_FEATURE(kHorizontalTabStripComboButton);

bool IsVerticalTabsFeatureEnabled();

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_FEATURES_H_
