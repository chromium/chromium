// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"

#include "base/win/hardware_check.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/cloud_synced_folder_checker.h"

namespace installer_downloader {

bool SystemInfoProviderImpl::IsHardwareEligibleForWin11() {
  return base::win::EvaluateWin11HardwareRequirements().IsEligible();
}

cloud_synced_folder_checker::CloudSyncStatus
SystemInfoProviderImpl::EvaluateOneDriveSyncStatus() {
  return cloud_synced_folder_checker::EvaluateOneDriveSyncStatus();
}

bool SystemInfoProviderImpl::IsOsEligible() {
  // Use Kernel32Version() here instead of base::win::GetVersion().
  // GetVersion() (via ::RtlGetVersion) can report a *downgraded* value when the
  // executable is running under Windows "application‑compatibility" shims or if
  // the binary lacks a supported‑OS manifest. The file version of kernel32.dll
  // is not subject to those version‑lies, so it gives us the true OS build
  // number and prevents spoofing.
  // See crbug.com/1404448.
  return base::win::OSInfo::Kernel32Version() < base::win::Version::WIN11;
}

}  // namespace installer_downloader
