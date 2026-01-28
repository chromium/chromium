// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace projects {

bool IsProjectsPanelEntrypointEnabled(const Profile* profile) {
  return tabs::IsProjectsPanelFeatureEnabled() &&
         profile->GetPrefs()->GetBoolean(
             prefs::kProjectsPanelEntrypointEnabled);
}

}  // namespace projects
