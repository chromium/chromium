// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/prefs/pref_service.h"

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate() = default;

ChromeCaptureModeDelegate::~ChromeCaptureModeDelegate() = default;

base::FilePath ChromeCaptureModeDelegate::GetActiveUserDownloadsDir() const {
  DCHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(ProfileManager::GetActiveUserProfile());
  return download_prefs->DownloadPath();
}

void ChromeCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {
  platform_util::ShowItemInFolder(ProfileManager::GetActiveUserProfile(),
                                  file_path);
}

bool ChromeCaptureModeDelegate::Uses24HourFormat() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // TODO(afakhry): Consider moving |prefs::kUse24HourClock| to ash/public so
  // we can do this entirely in ash.
  if (profile)
    return profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock);
  return base::GetHourClockType() == base::k24HourClock;
}
