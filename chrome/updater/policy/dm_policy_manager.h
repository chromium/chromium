// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// The DMPolicyManager returns device management policies for managed machines.
class DMPolicyManager : public PolicyManagerInterface {
 public:
  DMPolicyManager(
      const ::wireless_android_enterprise_devicemanagement::
          OmahaSettingsClientProto& omaha_settings,
      const absl::optional<bool>& override_is_managed_device = absl::nullopt);
  DMPolicyManager(const DMPolicyManager&) = delete;
  DMPolicyManager& operator=(const DMPolicyManager&) = delete;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  absl::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  absl::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  absl::optional<std::string> GetDownloadPreferenceGroupPolicy() const override;
  absl::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  absl::optional<int> GetPackageCacheExpirationTimeDays() const override;
  absl::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  absl::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  absl::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  absl::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  absl::optional<std::string> GetProxyMode() const override;
  absl::optional<std::string> GetProxyPacUrl() const override;
  absl::optional<std::string> GetProxyServer() const override;
  absl::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  absl::optional<std::vector<std::string>> GetForceInstallApps() const override;
  absl::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 private:
  ~DMPolicyManager() override;
  const ::wireless_android_enterprise_devicemanagement::ApplicationSettings*
  GetAppSettings(const std::string& app_id) const;

  const bool is_managed_device_;
  const ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings_;
};

// A factory method to create a DM policy manager.
scoped_refptr<PolicyManagerInterface> CreateDMPolicyManager(
    const absl::optional<bool>& override_is_managed_device);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
