// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_PREF_NAMES_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_PREF_NAMES_H_

namespace installer_downloader::prefs {

// Int browser local state that stores how many times the installer downloader
// inforbar has been shown.
inline constexpr char kInstallerDownloaderInfobarShowCount[] =
    "installer_downloader.infobar_show_count";

inline constexpr char kInstallerDownloaderInfobarLastShowTime[] =
    "installer_downloader.infobar_last_shown_time";

// Bool browser local state that indicates any future infobar display should be
// prevented even if the max show count is not reached.
inline constexpr char kInstallerDownloaderPreventFutureDisplay[] =
    "installer_downloader.prevent_future_display";

// Bool browser local state that indicates whether the installer downloader
// eligibility check should be by-passed.
inline constexpr char kInstallerDownloaderBypassEligibilityCheck[] =
    "installer_downloader.bypass_eligibility_check_for_testing";

}  // namespace installer_downloader::prefs

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_PREF_NAMES_H_
