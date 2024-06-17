// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/dm_policy_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/enterprise_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_message.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "device_management_backend.pb.h"

namespace updater {

namespace {

int PolicyValueFromProtoInstallDefaultValue(
    ::wireless_android_enterprise_devicemanagement::InstallDefaultValue
        install_default_value) {
  switch (install_default_value) {
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_DISABLED:
      return kPolicyDisabled;
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_ENABLED_MACHINE_ONLY:
      return kPolicyEnabledMachineOnly;
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_DEFAULT_ENABLED:
    default:
      return kPolicyEnabled;
  }
}

int PolicyValueFromProtoInstallValue(
    ::wireless_android_enterprise_devicemanagement::InstallValue
        install_value) {
  switch (install_value) {
    case ::wireless_android_enterprise_devicemanagement::INSTALL_DISABLED:
      return kPolicyDisabled;
    case ::wireless_android_enterprise_devicemanagement::
        INSTALL_ENABLED_MACHINE_ONLY:
      return kPolicyEnabledMachineOnly;
    case ::wireless_android_enterprise_devicemanagement::INSTALL_FORCED:
      return kPolicyForceInstallMachine;
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
        OmahaSettingsClientProto& omaha_settings,
    const std::optional<bool>& override_is_managed_device)
    : is_managed_device_(override_is_managed_device.value_or(true)),
      omaha_settings_(omaha_settings) {}

DMPolicyManager::~DMPolicyManager() = default;

bool DMPolicyManager::HasActiveDevicePolicies() const {
  return is_managed_device_;
}

std::string DMPolicyManager::source() const {
  return kSourceDMPolicyManager;
}

std::optional<bool> DMPolicyManager::CloudPolicyOverridesPlatformPolicy()
    const {
  if (!omaha_settings_.has_cloud_policy_overrides_platform_policy()) {
    return std::nullopt;
  }

  return omaha_settings_.cloud_policy_overrides_platform_policy();
}

std::optional<base::TimeDelta> DMPolicyManager::GetLastCheckPeriod() const {
  if (!omaha_settings_.has_auto_update_check_period_minutes()) {
    return std::nullopt;
  }

  return base::Minutes(omaha_settings_.auto_update_check_period_minutes());
}

std::optional<UpdatesSuppressedTimes>
DMPolicyManager::GetUpdatesSuppressedTimes() const {
  if (!omaha_settings_.has_updates_suppressed()) {
    return std::nullopt;
  }

  const auto& updates_suppressed = omaha_settings_.updates_suppressed();
  if (!updates_suppressed.has_start_hour() ||
      !updates_suppressed.has_start_minute() ||
      !updates_suppressed.has_duration_min()) {
    return std::nullopt;
  }

  UpdatesSuppressedTimes suppressed_times;
  suppressed_times.start_hour_ = updates_suppressed.start_hour();
  suppressed_times.start_minute_ = updates_suppressed.start_minute();
  suppressed_times.duration_minute_ = updates_suppressed.duration_min();
  return suppressed_times;
}

std::optional<std::string> DMPolicyManager::GetDownloadPreference() const {
  if (!omaha_settings_.has_download_preference()) {
    return std::nullopt;
  }

  return omaha_settings_.download_preference();
}

std::optional<int> DMPolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return std::nullopt;
}

std::optional<int> DMPolicyManager::GetPackageCacheExpirationTimeDays() const {
  return std::nullopt;
}

std::optional<std::string> DMPolicyManager::GetProxyMode() const {
  if (!omaha_settings_.has_proxy_mode()) {
    return std::nullopt;
  }

  return omaha_settings_.proxy_mode();
}

std::optional<std::string> DMPolicyManager::GetProxyPacUrl() const {
  if (!omaha_settings_.has_proxy_pac_url()) {
    return std::nullopt;
  }

  return omaha_settings_.proxy_pac_url();
}

std::optional<std::string> DMPolicyManager::GetProxyServer() const {
  if (!omaha_settings_.has_proxy_server()) {
    return std::nullopt;
  }

  return omaha_settings_.proxy_server();
}

const ::wireless_android_enterprise_devicemanagement::ApplicationSettings*
DMPolicyManager::GetAppSettings(const std::string& app_id) const {
  const auto& repeated_app_settings = omaha_settings_.application_settings();
  for (const auto& app_settings_proto : repeated_app_settings) {
#if BUILDFLAG(IS_MAC)
    // BundleIdentifier is preferred over AppGuid as product ID on Mac.
    // If not found, fall back to AppGuid below.
    if (app_settings_proto.has_bundle_identifier() &&
        base::EqualsCaseInsensitiveASCII(app_settings_proto.bundle_identifier(),
                                         app_id)) {
      return &app_settings_proto;
    }
#endif  // BUILDFLAG(IS_MAC)
    if (app_settings_proto.has_app_guid() &&
        base::EqualsCaseInsensitiveASCII(app_settings_proto.app_guid(),
                                         app_id)) {
      return &app_settings_proto;
    }
  }
  return nullptr;
}

std::optional<int> DMPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_install()) {
    return PolicyValueFromProtoInstallValue(app_settings->install());
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_install_default()) {
    return PolicyValueFromProtoInstallDefaultValue(
        omaha_settings_.install_default());
  }

  return std::nullopt;
}

std::optional<int> DMPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_update()) {
    return PolicyValueFromProtoUpdateValue(app_settings->update());
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_update_default()) {
    return PolicyValueFromProtoUpdateValue(omaha_settings_.update_default());
  }

  return std::nullopt;
}

std::optional<std::string> DMPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_version_prefix()) {
    return std::nullopt;
  }

  return app_settings->target_version_prefix();
}

std::optional<std::string> DMPolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_channel()) {
    return std::nullopt;
  }

  return app_settings->target_channel();
}

std::optional<bool> DMPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_rollback_to_target_version()) {
    return std::nullopt;
  }

  return (app_settings->rollback_to_target_version() ==
          ::wireless_android_enterprise_devicemanagement::
              ROLLBACK_TO_TARGET_VERSION_ENABLED);
}

std::optional<std::vector<std::string>> DMPolicyManager::GetForceInstallApps()
    const {
  std::vector<std::string> force_install_apps;
  for (const auto& app_settings_proto :
       omaha_settings_.application_settings()) {
    const std::string app_id = [&app_settings_proto, this] {
      if (app_settings_proto.install() != kPolicyForceInstallMachine &&
          omaha_settings_.install_default() != kPolicyForceInstallMachine) {
        return std::string();
      }
#if BUILDFLAG(IS_MAC)
      if (app_settings_proto.has_bundle_identifier()) {
        return app_settings_proto.bundle_identifier();
      }
#endif
      return app_settings_proto.app_guid();
    }();
    if (!app_id.empty()) {
      force_install_apps.push_back(app_id);
    }
  }
  return force_install_apps.empty()
             ? std::nullopt
             : std::optional<std::vector<std::string>>(force_install_apps);
}

std::optional<std::vector<std::string>> DMPolicyManager::GetAppsWithPolicy()
    const {
  std::vector<std::string> apps_with_policy;

  for (const auto& app_settings_proto :
       omaha_settings_.application_settings()) {
#if BUILDFLAG(IS_MAC)
    // BundleIdentifier is preferred over AppGuid as product ID on Mac.
    // If not found, fall back to AppGuid below.
    if (app_settings_proto.has_bundle_identifier()) {
      apps_with_policy.push_back(app_settings_proto.bundle_identifier());
      continue;
    }
#endif  // BUILDFLAG(IS_MAC)
    if (app_settings_proto.has_app_guid()) {
      apps_with_policy.push_back(app_settings_proto.app_guid());
    }
  }

  return apps_with_policy;
}

std::optional<
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
GetOmahaPolicySettings(
    scoped_refptr<device_management_storage::DMStorage> dm_storage) {
  wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  std::optional<enterprise_management::PolicyData> policy_data =
      dm_storage->ReadPolicyData(kGoogleUpdatePolicyType);
  if (!policy_data || !policy_data->has_policy_value()) {
    return std::nullopt;
  }
  if (!omaha_settings.ParseFromString(policy_data->policy_value())) {
    VLOG(1) << "Failed to parse OmahaSettingsClientProto";
    return std::nullopt;
  }
  return omaha_settings;
}

scoped_refptr<PolicyManagerInterface> CreateDMPolicyManager(
    const std::optional<bool>& override_is_managed_device) {
  scoped_refptr<device_management_storage::DMStorage> default_dm_storage =
      device_management_storage::GetDefaultDMStorage();
  if (!default_dm_storage) {
    return nullptr;
  }
  std::optional<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = GetOmahaPolicySettings(default_dm_storage);
  if (!omaha_settings) {
    return nullptr;
  }
  return base::MakeRefCounted<DMPolicyManager>(*omaha_settings,
                                               override_is_managed_device);
}

}  // namespace updater
