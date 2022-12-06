// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions_delegate_impl.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

using ::ash::assistant::AppStatus;

DeviceActionsDelegateImpl::DeviceActionsDelegateImpl() = default;

DeviceActionsDelegateImpl::~DeviceActionsDelegateImpl() = default;

AppStatus DeviceActionsDelegateImpl::GetAndroidAppStatus(
    const std::string& package_name) {
  const auto* prefs =
      ArcAppListPrefs::Get(ProfileManager::GetActiveUserProfile());
  if (!prefs) {
    LOG(ERROR) << "ArcAppListPrefs is not available.";
    return AppStatus::kUnknown;
  }
  std::string app_id = prefs->GetAppIdByPackageName(package_name);

  return app_id.empty() ? AppStatus::kUnavailable : AppStatus::kAvailable;
}
