// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_IMPL_H_

#include "chrome/browser/win/cloud_synced_folder_checker.h"
#include "chrome/browser/win/installer_downloader/system_info_provider.h"

namespace installer_downloader {

class SystemInfoProviderImpl final : public SystemInfoProvider {
 public:
  SystemInfoProviderImpl() = default;
  SystemInfoProviderImpl(const SystemInfoProviderImpl&) = delete;
  SystemInfoProviderImpl& operator=(const SystemInfoProviderImpl&) = delete;

  ~SystemInfoProviderImpl() override = default;

  // SystemInfoProvider:
  bool IsHardwareEligibleForWin11() override;
  cloud_synced_folder_checker::CloudSyncStatus EvaluateOneDriveSyncStatus()
      override;
  bool IsOsEligible() override;
};

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_SYSTEM_INFO_PROVIDER_IMPL_H_
