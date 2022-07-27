// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"

namespace updater {

// The DMPolicyManager returns device management policies for managed machines.
class DMPolicyManager : public PolicyManagerInterface {
 public:
  explicit DMPolicyManager(
      const ::wireless_android_enterprise_devicemanagement::
          OmahaSettingsClientProto& omaha_settings);
  DMPolicyManager(const DMPolicyManager&) = delete;
  DMPolicyManager& operator=(const DMPolicyManager&) = delete;
  ~DMPolicyManager() override;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  bool GetLastCheckPeriodMinutes(int* minutes) const override;
  bool GetUpdatesSuppressedTimes(
      UpdatesSuppressedTimes* suppressed_times) const override;
  bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const override;
  bool GetProxyMode(std::string* proxy_mode) const override;
  bool GetProxyPacUrl(std::string* proxy_pac_url) const override;
  bool GetProxyServer(std::string* proxy_server) const override;
  bool GetPackageCacheSizeLimitMBytes(int* cache_size_limit) const override;
  bool GetPackageCacheExpirationTimeDays(int* cache_life_limit) const override;

  bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                        int* install_policy) const override;
  bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                       int* update_policy) const override;
  bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const override;
  bool GetTargetChannel(const std::string& app_id,
                        std::string* channel) const override;
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        bool* rollback_allowed) const override;
  bool GetForceInstallApps(
      std::vector<std::string>* force_install_apps) const override;

 private:
  const ::wireless_android_enterprise_devicemanagement::ApplicationSettings*
  GetAppSettings(const std::string& app_id) const;

  const ::wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings_;
};

// A factory method to create a DM policy manager.
std::unique_ptr<PolicyManagerInterface> CreateDMPolicyManager();

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_DM_POLICY_MANAGER_H_
