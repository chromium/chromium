// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace webui {

void OpenNewWindowForProfile(Profile* profile) {
  if (profiles::IsProfileLocked(profile->GetPath())) {
    DCHECK(signin_util::IsForceSigninEnabled());
    // Displays the ProfilePicker without any sign-in dialog opened.
    if (!ProfilePicker::IsOpen()) {
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileLocked));
    }

    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::OnceCallback<void(Profile*)>());
    return;
  }

  if (ProfilePicker::IsOpen()) {
    // If the profile picker is open, do not open a new browser automatically.
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion));
    return;
  }

  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IsProcessStartup::kYes,
      chrome::startup::IsFirstRun::kYes, false);
}

void DeleteProfileAtPath(base::FilePath file_path,
                         ProfileMetrics::ProfileDelete deletion_source) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          file_path, base::BindOnce(&OpenNewWindowForProfile), deletion_source);
}

}  // namespace webui
