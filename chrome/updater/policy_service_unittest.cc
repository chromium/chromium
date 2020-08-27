// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "chrome/updater/policy_manager.h"
#include "chrome/updater/policy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// The Policy Manager Interface is implemented by policy managers such as Group
// Policy and Device Management.
class FakePolicyManager : public PolicyManagerInterface {
 public:
  ~FakePolicyManager() override = default;

  std::string source() const override { return source_; }
  bool IsManaged() const override { return managed_; }
  bool GetLastCheckPeriodMinutes(int* minutes) const override { return false; }
  bool GetUpdatesSuppressedTimes(int* start_hour,
                                 int* start_min,
                                 int* duration_min) const override {
    return false;
  }
  bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const override {
    return false;
  }
  bool GetPackageCacheSizeLimitMBytes(int* cache_size_limit) const override {
    return false;
  }
  bool GetPackageCacheExpirationTimeDays(int* cache_life_limit) const override {
    return false;
  }
  bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                        int* install_policy) const override {
    return false;
  }
  bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                       int* update_policy) const override {
    return false;
  }
  bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const override {
    return false;
  }
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        bool* rollback_allowed) const override {
    return false;
  }
  bool GetProxyMode(std::string* proxy_mode) const override { return false; }
  bool GetProxyPacUrl(std::string* proxy_pac_url) const override {
    return false;
  }
  bool GetProxyServer(std::string* proxy_server) const override {
    return false;
  }
  bool GetTargetChannel(const std::string& app_id,
                        std::string* channel) const override {
    auto value = channels_.find(app_id);
    if (value == channels_.end())
      return false;
    *channel = value->second;
    return true;
  }
  void SetChannel(const std::string& app_id, std::string channel) {
    channels_[app_id] = std::move(channel);
  }

  static std::unique_ptr<FakePolicyManager> GetTestingPolicyManager(
      std::string source,
      bool managed) {
    auto manager = std::make_unique<FakePolicyManager>();
    manager->source_ = std::move(source);
    manager->managed_ = managed;
    return manager;
  }

 private:
  std::string source_;
  std::map<std::string, std::string> channels_;
  bool managed_;
};

TEST(PolicyService, ReturnsHighestPriorityManagedPolicyManager) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.emplace_back(
      FakePolicyManager::GetTestingPolicyManager("highest_unmanaged", false));
  managers.emplace_back(
      FakePolicyManager::GetTestingPolicyManager("highest_managed", true));
  managers.emplace_back(
      FakePolicyManager::GetTestingPolicyManager("managed", true));
  managers.emplace_back(
      FakePolicyManager::GetTestingPolicyManager("lowest_managed", true));
  managers.emplace_back(
      FakePolicyManager::GetTestingPolicyManager("lowest_unmanaged", false));
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  ASSERT_EQ("highest_managed",
            policy_service->GetActivePolicyManager().source());
}

TEST(PolicyService, ReturnsDefaultPolicyManager) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  policy_service->SetPolicyManagersForTesting({});
  ASSERT_EQ("default", policy_service->GetActivePolicyManager().source());
}

TEST(PolicyService, TargetChannelUnmanagedSource) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  auto manager = FakePolicyManager::GetTestingPolicyManager("unmanaged", false);
  manager->SetChannel("", "channel");
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.emplace_back(std::move(manager));
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  std::string channel;
  policy_service->GetTargetChannel("", &channel);
  ASSERT_TRUE(channel.empty());
}

TEST(PolicyService, TargetChannelManagedSource) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  auto manager = FakePolicyManager::GetTestingPolicyManager("managed", true);
  manager->SetChannel("", "channel");
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.emplace_back(std::move(manager));
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  std::string channel;
  policy_service->GetTargetChannel("", &channel);
  ASSERT_EQ(channel, "channel");
}

}  // namespace updater
