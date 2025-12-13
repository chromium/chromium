// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"

#include "base/command_line.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace session_restore_infobar {

SessionRestoreInfobarModel::~SessionRestoreInfobarModel() = default;

SessionRestoreInfobarModel::SessionRestoreInfobarModel(
    Profile& profile,
    bool is_post_crash_launch)
    : profile_(profile),
      is_post_crash_launch_(is_post_crash_launch),
      initial_restore_on_startup_value_(
          profile_->GetPrefs()->GetInteger(prefs::kRestoreOnStartup)) {}

SessionRestoreInfobarModel::SessionRestoreMessageValue
SessionRestoreInfobarModel::GetSessionRestoreMessageValue() const {
  // Get the integer value from the user's profile preferences.
  int restore_on_startup_value =
      profile_->GetPrefs()->GetInteger(prefs::kRestoreOnStartup);
  // Get the value for chrome session restore.
  switch (restore_on_startup_value) {
    case 1:
      return SessionRestoreMessageValue::kContinueWhereLeftOff;
    case 4:
      return SessionRestoreMessageValue::kOpenSpecificPages;
    case 5:
      return SessionRestoreMessageValue::kOpenNewTabPage;
    default:
      return SessionRestoreMessageValue::kOpenNewTabPage;
  }
}

bool SessionRestoreInfobarModel::ShouldShowOnStartup() const {
  if (is_post_crash_launch_) {
    return false;
  }

  SessionRestoreMessageValue message_value = GetSessionRestoreMessageValue();

  return message_value == SessionRestoreMessageValue::kContinueWhereLeftOff ||
         message_value == SessionRestoreMessageValue::kOpenNewTabPage;
}


bool SessionRestoreInfobarModel::IsDefaultSessionRestorePref() const {
  const PrefService::Preference* pref =
      profile_->GetPrefs()->FindPreference(prefs::kRestoreOnStartup);
  CHECK(pref);
  return pref->IsDefaultValue();
}

bool SessionRestoreInfobarModel::HasSessionRestoreSettingChanged(
    const PrefService& prefs) const {
  return initial_restore_on_startup_value_ !=
         prefs.GetInteger(prefs::kRestoreOnStartup);
}

}  // namespace session_restore_infobar
