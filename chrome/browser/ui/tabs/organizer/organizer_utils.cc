// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"

namespace organizer {

bool IsOrganizerPanelEntrypointEnabled(const Profile* profile) {
  return tab_groups::IsOrganizerPanelFeatureEnabled() &&
         profile->GetPrefs()->GetBoolean(
             prefs::kOrganizerPanelEntrypointEnabled);
}

}  // namespace organizer
