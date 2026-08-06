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

// Int browser local state that stores how many re-engagement campaign cycles
// have been started for the user.
inline constexpr char kInstallerDownloaderCycleCount[] =
    "installer_downloader.cycle_count";

// Bool browser local state that indicates whether the installer download was
// completed successfully, permanently suppressing future displays across all
// re-engagement cycles.
inline constexpr char kInstallerDownloaderDownloadCompleted[] =
    "installer_downloader.download_completed";

// Int browser local state that stores the total number of times the installer
// downloader infobar has been shown across all cycles.
inline constexpr char kInstallerDownloaderTotalShowCount[] =
    "installer_downloader.total_show_count";

}  // namespace installer_downloader::prefs

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_PREF_NAMES_H_
