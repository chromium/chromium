// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"

namespace updater {

// The DMPolicyManager returns device management policies for managed machines.
class DMPolicyManager : public PolicyManagerInterface {
 public:
  explicit DMPolicyManager(
      const ::wireless_android_enterprise_devicemanagement::
          OmahaSettingsClientProto& omaha_settings,
      const std::optional<bool>& override_is_managed_device = std::nullopt);
  DMPolicyManager(const DMPolicyManager&) = delete;
  DMPolicyManager& operator=(const DMPolicyManager&) = delete;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  std::optional<bool> CloudPolicyOverridesPlatformPolicy() const override;
  std::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  std::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  std::optional<std::string> GetDownloadPreference() const override;
  std::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  std::optional<int> GetPackageCacheExpirationTimeDays() const override;
  std::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  std::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  std::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  std::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  std::optional<std::string> GetProxyMode() const override;
  std::optional<std::string> GetProxyPacUrl() const override;
  std::optional<std::string> GetProxyServer() const override;
  std::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  std::optional<std::vector<std::string>> GetForceInstallApps() const override;
  std::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 private:
  ~DMPolicyManager() override;
  const ::wireless_android_enterprise_devicemanagement::ApplicationSettings*
  GetAppSettings(const std::string& app_id) const;

  const bool is_managed_device_;
  const ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings_;
};

// Read the Omaha settings from DM storage.
std::optional<
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto>
GetOmahaPolicySettings(
    scoped_refptr<device_management_storage::DMStorage> dm_storage);

// A factory method to create a DM policy manager.
scoped_refptr<PolicyManagerInterface> CreateDMPolicyManager(
    const std::optional<bool>& override_is_managed_device);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
