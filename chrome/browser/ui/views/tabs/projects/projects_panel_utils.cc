// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/saved_tab_groups/public/features.h"

namespace projects_panel {

bool IsProjectsPanelVisibleForProfile(Profile* profile) {
  return tab_groups::IsProjectsPanelFeatureEnabled() &&
         tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile);
}

}  // namespace projects_panel
