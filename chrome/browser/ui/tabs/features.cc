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

BASE_FEATURE(kVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabSelectionByPointer, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProjectsPanel, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVerticalTabsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kVerticalTabs);
}

bool IsProjectsPanelFeatureEnabled() {
  return base::FeatureList::IsEnabled(kProjectsPanel);
}

}  // namespace tabs
