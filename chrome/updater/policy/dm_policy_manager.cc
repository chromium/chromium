// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/dm_policy_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/enterprise_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"

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

bool DMPolicyManager::HasActiveDevicePolicies() const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::IsManagedDevice();
#else
  // crbug.com/1276162 - implement.
  NOTIMPLEMENTED();
  return false;
#endif
}

std::string DMPolicyManager::source() const {
  return kSourceDMPolicyManager;
}

absl::optional<base::TimeDelta> DMPolicyManager::GetLastCheckPeriod() const {
  if (!omaha_settings_.has_auto_update_check_period_minutes())
    return absl::nullopt;

  return base::Minutes(omaha_settings_.auto_update_check_period_minutes());
}

absl::optional<UpdatesSuppressedTimes>
DMPolicyManager::GetUpdatesSuppressedTimes() const {
  if (!omaha_settings_.has_updates_suppressed())
    return absl::nullopt;

  const auto& updates_suppressed = omaha_settings_.updates_suppressed();
  if (!updates_suppressed.has_start_hour() ||
      !updates_suppressed.has_start_minute() ||
      !updates_suppressed.has_duration_min())
    return absl::nullopt;

  UpdatesSuppressedTimes suppressed_times;
  suppressed_times.start_hour_ = updates_suppressed.start_hour();
  suppressed_times.start_minute_ = updates_suppressed.start_minute();
  suppressed_times.duration_minute_ = updates_suppressed.duration_min();
  return suppressed_times;
}

absl::optional<std::string> DMPolicyManager::GetDownloadPreferenceGroupPolicy()
    const {
  if (!omaha_settings_.has_download_preference())
    return absl::nullopt;

  return omaha_settings_.download_preference();
}

absl::optional<int> DMPolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return absl::nullopt;
}

absl::optional<int> DMPolicyManager::GetPackageCacheExpirationTimeDays() const {
  return absl::nullopt;
}

absl::optional<std::string> DMPolicyManager::GetProxyMode() const {
  if (!omaha_settings_.has_proxy_mode())
    return absl::nullopt;

  return omaha_settings_.proxy_mode();
}

absl::optional<std::string> DMPolicyManager::GetProxyPacUrl() const {
  if (!omaha_settings_.has_proxy_pac_url())
    return absl::nullopt;

  return omaha_settings_.proxy_pac_url();
}

absl::optional<std::string> DMPolicyManager::GetProxyServer() const {
  if (!omaha_settings_.has_proxy_server())
    return absl::nullopt;

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

absl::optional<int> DMPolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_install()) {
    return PolicyValueFromProtoInstallValue(app_settings->install());
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_install_default()) {
    return PolicyValueFromProtoInstallValue(omaha_settings_.install_default());
  }

  return absl::nullopt;
}

absl::optional<int> DMPolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (app_settings && app_settings->has_update()) {
    return PolicyValueFromProtoUpdateValue(app_settings->update());
  }

  // Fallback to global-level settings.
  if (omaha_settings_.has_update_default()) {
    return PolicyValueFromProtoUpdateValue(omaha_settings_.update_default());
  }

  return absl::nullopt;
}

absl::optional<std::string> DMPolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_version_prefix())
    return absl::nullopt;

  return app_settings->target_version_prefix();
}

absl::optional<std::string> DMPolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_target_channel())
    return absl::nullopt;

  return app_settings->target_channel();
}

absl::optional<bool> DMPolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  const auto* app_settings = GetAppSettings(app_id);
  if (!app_settings || !app_settings->has_rollback_to_target_version())
    return absl::nullopt;

  return (app_settings->rollback_to_target_version() ==
          ::wireless_android_enterprise_devicemanagement::
              ROLLBACK_TO_TARGET_VERSION_ENABLED);
}

// TODO(crbug.com/1347562): implement retrieving the force installs apps.
absl::optional<std::vector<std::string>> DMPolicyManager::GetForceInstallApps()
    const {
  return absl::nullopt;
}

scoped_refptr<PolicyManagerInterface> CreateDMPolicyManager() {
  std::unique_ptr<
      ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
      omaha_settings = GetDefaultDMStorage()->GetOmahaPolicySettings();
  if (!omaha_settings)
    return nullptr;

  return base::MakeRefCounted<DMPolicyManager>(*omaha_settings);
}

}  // namespace updater
