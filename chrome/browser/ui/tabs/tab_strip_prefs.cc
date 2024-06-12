// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_prefs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace tabs {

bool GetDefaultTabSearchRightAligned() {
  // These platforms are all left aligned, the others should be right.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return false;
#else
  return true;
#endif
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kTabSearchRightAligned,
                                GetDefaultTabSearchRightAligned());
}

bool GetTabSearchRightAligned(const Profile* profile) {
  // If the setting isn't enabled, just use the default behavior.
  if (!CanShowTabSearchPositionSetting()) {
    return GetDefaultTabSearchRightAligned();
  }

  return profile->GetPrefs()->GetBoolean(prefs::kTabSearchRightAligned);
}

}  // namespace tabs
