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
  explicit FakePolicyManager(const std::string& source) : source_(source) {}
  ~FakePolicyManager() override = default;

  std::string source() const override { return source_; }
  bool IsManaged() const override { return true; }
  bool GetLastCheckPeriodMinutes(int* minutes) const override { return false; }
  bool GetUpdatesSuppressedTimes(int* start_hour,
                                 int* start_min,
                                 int* duration_min) const override {
    return false;
  }
  bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const override {
    if (download_preference_.empty())
      return false;

    *download_preference = download_preference_;
    return true;
  }
  void SetDownloadPreferenceGroupPolicy(const std::string& preference) {
    download_preference_ = preference;
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
    auto value = update_policies_.find(app_id);
    if (value == update_policies_.end())
      return false;
    *update_policy = value->second;
    return true;
  }
  void SetUpdatePolicy(const std::string& app_id, int update_policy) {
    update_policies_[app_id] = update_policy;
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

 private:
  std::string source_;
  std::string download_preference_;
  std::map<std::string, std::string> channels_;
  std::map<std::string, int> update_policies_;
};

TEST(PolicyService, DefaultPolicyValue) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.push_back(GetPolicyManager());
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(), "");

  std::string version_prefix;
  EXPECT_FALSE(policy_service->GetTargetVersionPrefix("", &version_prefix));
  int last_check = 0;
  EXPECT_FALSE(policy_service->GetLastCheckPeriodMinutes(&last_check));
}

TEST(PolicyService, SinglePolicyManager) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  auto manager = std::make_unique<FakePolicyManager>("test_source");
  manager->SetChannel("app1", "test_channel");
  manager->SetUpdatePolicy("app2", 3);
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.push_back(std::move(manager));
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(), "test_source");

  std::string channel;
  EXPECT_FALSE(policy_service->GetTargetChannel("app2", &channel));
  EXPECT_TRUE(policy_service->GetTargetChannel("app1", &channel));
  EXPECT_EQ(channel, "test_channel");

  int update_policy = 0;
  EXPECT_FALSE(
      policy_service->GetEffectivePolicyForAppUpdates("app1", &update_policy));
  EXPECT_TRUE(
      policy_service->GetEffectivePolicyForAppUpdates("app2", &update_policy));
  EXPECT_EQ(update_policy, 3);
}

TEST(PolicyService, MultiplePolicyManagers) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;

  auto manager = std::make_unique<FakePolicyManager>("group_policy");
  manager->SetChannel("app1", "channel_gp");
  manager->SetUpdatePolicy("app2", 1);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>("device_management");
  manager->SetChannel("app1", "channel_dm");
  manager->SetUpdatePolicy("app1", 3);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>("imaginary");
  manager->SetChannel("app1", "channel_imaginary");
  manager->SetUpdatePolicy("app1", 2);
  manager->SetDownloadPreferenceGroupPolicy("cacheable");
  managers.push_back(std::move(manager));

  // The default policy manager.
  managers.push_back(GetPolicyManager());

  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(),
            "group_policy;device_management;imaginary");

  std::string channel;
  EXPECT_TRUE(policy_service->GetTargetChannel("app1", &channel));
  EXPECT_EQ(channel, "channel_gp");

  int update_policy = 0;
  EXPECT_TRUE(
      policy_service->GetEffectivePolicyForAppUpdates("app1", &update_policy));
  EXPECT_EQ(update_policy, 3);
  EXPECT_TRUE(
      policy_service->GetEffectivePolicyForAppUpdates("app2", &update_policy));
  EXPECT_EQ(update_policy, 1);

  std::string download_preference;
  EXPECT_TRUE(
      policy_service->GetDownloadPreferenceGroupPolicy(&download_preference));
  EXPECT_EQ(download_preference, "cacheable");

  int cache_size_limit = 0;
  EXPECT_FALSE(
      policy_service->GetPackageCacheSizeLimitMBytes(&cache_size_limit));
}

}  // namespace updater
