// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONSTANTS_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONSTANTS_H_

namespace installer_downloader {

// Default URL for the 'Learn More' link in the infobar.
inline constexpr char kLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=win10_transition";

// Indicates the file name of the downloaded installer.
inline constexpr char kDownloadedInstallerFileName[] = "ChromeSetup.exe";

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_CONSTANTS_H_
