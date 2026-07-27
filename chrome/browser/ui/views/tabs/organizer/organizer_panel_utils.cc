// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/saved_tab_groups/public/features.h"

namespace organizer_panel {

bool IsOrganizerPanelVisibleForProfile(Profile* profile) {
  return tab_groups::IsOrganizerPanelFeatureEnabled() &&
         tab_groups::SavedTabGroupUtils::IsEnabledForProfile(profile);
}

}  // namespace organizer_panel
