// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"

#include "base/feature.h"

namespace organizer_panel {

BASE_FEATURE(kOrganizerPanel, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOrganizerPanelForeignTabs, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kShowExtensionsSidePanelUiInOrganizerPanel,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOrganizerPanelFeatureEnabled() {
  return base::FeatureList::IsEnabled(kOrganizerPanel);
}

bool IsOrganizerPanelForeignTabsEnabled() {
  return IsOrganizerPanelFeatureEnabled() &&
         base::FeatureList::IsEnabled(kOrganizerPanelForeignTabs);
}

bool IsShowExtensionsSidePanelUiInOrganizerPanelEnabled() {
  return base::FeatureList::IsEnabled(
      kShowExtensionsSidePanelUiInOrganizerPanel);
}

}  // namespace organizer_panel
