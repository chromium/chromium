// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_policy_manager.h"

#include "base/enterprise_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"

namespace updater {

namespace {

int PolicyValueFromProtoInstallValue(
    ::wireless_android_enterprise_devicemanagement::InstallValue
        install_value) {
  switch (install_value) {
    case ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED:
      return kPolicyDisabled;

    case ::wireless_android_enterprise_devicemanagement::INSTALL_ENABLED:
    default:
      return kPolicyEnabled;
  }
}

int PolicyValueFromProtoUpdateValue(
    ::wireless_android_enterprise_devicemanagement::UpdateValue update_value) {
  switch (update_value) {
    case ::wireless_android_enterprise_devicemanagement::UPDATES_DISABLED:
      return kPolicyDisabled;

    case ::wireless_android_enterprise_devicemanagement::MANUAL_UPDATES_ONLY:
      return kPolicyManualUpdatesOnly;

    case ::wireless_android_enterprise_devicemanagement::AUTOMATIC_UPDATES_ONLY:
      return kPolicyAutomaticUpdatesOnly;

    case ::wireless_android_enterprise_devicemanagement::UPDATES_ENABLED:
    default:
      return kPolicyEnabled;
  }
}

}  // namespace

DMPolicyManager::DMPolicyManager(
    const ::wireless_android_enterprise_devicemanagement::
        OmahaSettingsClientProto& omaha_settings)
    : omaha_settings_(omaha_settings) {}

DMPolicyManager::~DMPolicyManager() = default;

bool DMPolicyManager::IsManaged() const {
  return true;
}

std::string DMPolicyManager::source() const {
  return std::string("DeviceManagement");
}

bool DMPolicyManager::GetLastCheckPeriodMinutes(int* minutes) const {
  if (!omaha_settings_.has_auto_update_check_period_minutes())
    return false;

  *minutes =
      static_cast<int>(omaha_settings_.auto_update_check_period_minutes());
  return true;
}

bool DMPolicyManager::GetUpdatesSuppressedTimes(int* start_hour,
                                                int* start_min,
                                                int* duration_min) const {
  if (!omaha_settings_.has_updates_suppressed())
    return false;

  const auto& updates_suppressed = omaha_settings_.updates_suppressed();
  if (!updates_suppressed.has_start_hour() ||
      !updates_suppressed.has_start_minute() ||
      !updates_suppressed.has_duration_min())
    return false;

  *start_hour = updates_suppressed.start_hour();
  *start_min = updates_suppressed.start_minute();
  *duration_min = updates_suppressed.duration_min();
  return true;
}

bool DMPolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  if (!omaha_settings_.has_download_preference())
    return false;

  *download_preference = omaha_settings_.download_preference();
  return true;
}

bool DMPolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return false;
}

bool DMPolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return false;
}

bool DMPolicyManager::GetProxyMode(std::string* proxy_mode) const {
  if (!omaha_settings_.has_proxy_mode())
    return false;

  *proxy_mode = omaha_settings_.proxy_mode();
  return true;
}

bool DMPolicyManager::GetProxyPacUrl(std::string* proxy_pac_url) const {
  if (!omaha_settings_.has_proxy_pac_url())
    return false;

  *proxy_pac_url = omaha_settings_.proxy_pac_url();
  return true;
}

bool DMPolicyManager::GetProxyServer(std::string* proxy_server) const {
  if (!omaha_settings_.has_proxy_server())
    return false;

  *proxy_server = omaha_settings_.proxy_server();
  return true;
}

const ::wireless_android_enterprise_devicemanagement::ApplicationSettings*
DMPolicyManager::GetAppSettings(const std::string& app_id) const {
  const auto& repeated_app_settings = omaha_settings_.application_settings();
  for (const auto& app_settings_proto : repeated_app_settings) {
#if defined(OS_MAC)
    // BundleIdentifier is preferred over AppGuid as product ID on Mac.
    // If not found, fall back to AppGuid below.
    if (app_settings_proto.has_bundle_identifier() &&
        base::EqualsCaseInsensitiveASCII(app_settings_proto.bundle_identifier(),
                                         app_id)) {
      return &app_settings_proto;
    }
#endif  // OS_MAC
    if (app_settings_proto.has_app_guid() &&
        base::EqualsCaseInsensitiveASCII(app_settings_proto.app_guid(),
                                         app_id)) {
      return &app_settings_proto;
    }
  }
  return nullptr;
}

bool DMPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_install()) {
    *install_policy = PolicyValueFromProtoInstallValue(app_settings->install());
    return true;
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_install_default()) {
    *install_policy =
        PolicyValueFromProtoInstallValue(omaha_settings_.install_default());
    return true;
  }

  return false;
}

bool DMPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_update()) {
    *update_policy = PolicyValueFromProtoUpdateValue(app_settings->update());
    return true;
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_update_default()) {
    *update_policy =
        PolicyValueFromProtoUpdateValue(omaha_settings_.update_default());
    return true;
  }

  return false;
}

bool DMPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_version_prefix())
    return false;

  *target_version_prefix = app_settings->target_version_prefix();
  return true;
}

bool DMPolicyManager::GetTargetChannel(const std::string& app_id,
                                       std::string* channel) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_channel())
    return false;

  *channel = app_settings->target_channel();
  return true;
}

bool DMPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_rollback_to_target_version())
    return false;

  *rollback_allowed = (app_settings->rollback_to_target_version() ==
                       ::wireless_android_enterprise_devicemanagement::
                           ROLLBACK_TO_TARGET_VERSION_ENABLED);
  return true;
}

}  // namespace updater
