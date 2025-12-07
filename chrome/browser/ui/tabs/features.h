// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_FEATURES_H_

#include "base/feature_list.h"

namespace tabs {

// TODO(crbug.com/346837232): move all flags to this file.

BASE_DECLARE_FEATURE(kDebugUITabStrip);

BASE_DECLARE_FEATURE(kTabGroupHome);

BASE_DECLARE_FEATURE(kTabSearchPositionSetting);

BASE_DECLARE_FEATURE(kVerticalTabs);

BASE_DECLARE_FEATURE(kTabSelectionByPointer);

bool CanShowTabSearchPositionSetting();
bool IsVerticalTabsFeatureEnabled();

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_FEATURES_H_
