// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"

namespace organizer_panel {

BASE_FEATURE(kOrganizerPanel, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOrganizerPanelFeatureEnabled() {
  return base::FeatureList::IsEnabled(kOrganizerPanel);
}

}  // namespace organizer_panel
