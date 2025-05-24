// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_H_

namespace cloud_synced_folder_checker {
struct CloudSyncStatus;
}

namespace installer_downloader {

// Abstraction layer over OS / OneDrive queries so that they can be mocked
// in unit and end‑to‑end tests.
class SystemInfoProvider {
 public:
  virtual ~SystemInfoProvider() = default;

  // Returns true if the device meets Win 11 hardware requirements.
  virtual bool IsHardwareEligibleForWin11() = 0;

  // Returns OneDrive sync status for Desktop and OneDrive root.
  virtual cloud_synced_folder_checker::CloudSyncStatus
  EvaluateOneDriveSyncStatus() = 0;

  // Returns true when the running OS version is *prior* to Windows 11.
  virtual bool IsOsEligible() = 0;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_H_
