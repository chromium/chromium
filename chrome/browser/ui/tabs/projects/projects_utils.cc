// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"

namespace projects {

bool IsProjectsPanelEntrypointEnabled(const Profile* profile) {
  return tab_groups::IsProjectsPanelFeatureEnabled() &&
         profile->GetPrefs()->GetBoolean(
             prefs::kProjectsPanelEntrypointEnabled);
}

}  // namespace projects
