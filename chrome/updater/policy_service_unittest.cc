// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/updater/policy_manager.h"
#include "chrome/updater/policy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// The Policy Manager Interface is implemented by policy managers such as Group
// Policy and Device Management.
class FakePolicyManager : public PolicyManagerInterface {
 public:
  FakePolicyManager(bool is_managed, const std::string& source)
      : is_managed_(is_managed), source_(source) {}
  ~FakePolicyManager() override = default;

  std::string source() const override { return source_; }
  bool IsManaged() const override { return is_managed_; }
  bool GetLastCheckPeriodMinutes(int* minutes) const override { return false; }
  bool GetUpdatesSuppressedTimes(
      UpdatesSuppressedTimes* suppressed_times) const override {
    if (!suppressed_times_.valid())
      return false;

    *suppressed_times = suppressed_times_;
    return true;
  }
  void SetUpdatesSuppressedTimes(
      const UpdatesSuppressedTimes& suppressed_times) {
    suppressed_times_ = suppressed_times;
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
  bool is_managed_;
  std::string source_;
  UpdatesSuppressedTimes suppressed_times_;
  std::string download_preference_;
  std::map<std::string, std::string> channels_;
  std::map<std::string, int> update_policies_;
};

TEST(PolicyService, DefaultPolicyValue) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.push_back(GetPolicyManager());
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(), "default");

  std::string version_prefix;
  EXPECT_FALSE(
      policy_service->GetTargetVersionPrefix("", nullptr, &version_prefix));
  int last_check = 0;
  EXPECT_FALSE(policy_service->GetLastCheckPeriodMinutes(nullptr, &last_check));
}

TEST(PolicyService, SinglePolicyManager) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  auto manager = std::make_unique<FakePolicyManager>(true, "test_source");
  manager->SetChannel("app1", "test_channel");
  manager->SetUpdatePolicy("app2", 3);
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;
  managers.push_back(std::move(manager));
  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(), "test_source");

  PolicyStatus<std::string> app1_channel_status;
  std::string app1_channel;
  EXPECT_TRUE(policy_service->GetTargetChannel("app1", &app1_channel_status,
                                               &app1_channel));
  EXPECT_TRUE(app1_channel_status.effective_policy());
  EXPECT_EQ(app1_channel_status.effective_policy().value().policy,
            "test_channel");
  EXPECT_EQ(app1_channel, "test_channel");
  EXPECT_FALSE(app1_channel_status.conflict_policy());

  PolicyStatus<std::string> app2_channel_status;
  std::string app2_channel;
  EXPECT_FALSE(policy_service->GetTargetChannel("app2", &app2_channel_status,
                                                &app2_channel));
  EXPECT_FALSE(app2_channel_status.effective_policy());
  EXPECT_FALSE(app2_channel_status.conflict_policy());

  PolicyStatus<int> app1_update_status;
  int update_policy = 0;
  EXPECT_FALSE(policy_service->GetEffectivePolicyForAppUpdates(
      "app1", &app1_update_status, &update_policy));
  EXPECT_FALSE(app1_update_status.conflict_policy());

  PolicyStatus<int> app2_update_status;
  EXPECT_TRUE(policy_service->GetEffectivePolicyForAppUpdates(
      "app2", &app2_update_status, &update_policy));
  EXPECT_TRUE(app2_update_status.effective_policy());
  EXPECT_EQ(app2_update_status.effective_policy().value().policy, 3);
  EXPECT_FALSE(app2_update_status.conflict_policy());
  EXPECT_EQ(update_policy, 3);
}

TEST(PolicyService, MultiplePolicyManagers) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;

  auto manager = std::make_unique<FakePolicyManager>(true, "group_policy");
  UpdatesSuppressedTimes updates_suppressed_times = {5, 10, 30};
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_gp");
  manager->SetUpdatePolicy("app2", 1);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>(true, "device_management");
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_dm");
  manager->SetUpdatePolicy("app1", 3);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>(true, "imaginary");
  updates_suppressed_times = {1, 1, 20};
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_imaginary");
  manager->SetUpdatePolicy("app1", 2);
  manager->SetDownloadPreferenceGroupPolicy("cacheable");
  managers.push_back(std::move(manager));

  // The default policy manager.
  managers.push_back(GetPolicyManager());

  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(),
            "group_policy;device_management;imaginary;default");

  PolicyStatus<UpdatesSuppressedTimes> suppressed_time_status;
  EXPECT_TRUE(policy_service->GetUpdatesSuppressedTimes(
      &suppressed_time_status, &updates_suppressed_times));
  EXPECT_TRUE(suppressed_time_status.conflict_policy());
  EXPECT_EQ(suppressed_time_status.effective_policy().value().source,
            "group_policy");
  EXPECT_EQ(updates_suppressed_times.start_hour, 5);
  EXPECT_EQ(updates_suppressed_times.start_minute, 10);
  EXPECT_EQ(updates_suppressed_times.duration_minute, 30);

  PolicyStatus<std::string> channel_status;
  std::string channel;
  EXPECT_TRUE(
      policy_service->GetTargetChannel("app1", &channel_status, &channel));
  const PolicyStatus<std::string>::Entry& channel_policy =
      channel_status.effective_policy().value();
  EXPECT_EQ(channel_policy.source, "group_policy");
  EXPECT_EQ(channel_policy.policy, "channel_gp");
  EXPECT_TRUE(channel_status.conflict_policy());
  const PolicyStatus<std::string>::Entry& channel_conflict_policy =
      channel_status.conflict_policy().value();
  EXPECT_EQ(channel_conflict_policy.source, "device_management");
  EXPECT_EQ(channel_conflict_policy.policy, "channel_dm");
  EXPECT_EQ(channel, "channel_gp");

  PolicyStatus<int> app1_update_status;
  int update_policy = 0;
  EXPECT_TRUE(policy_service->GetEffectivePolicyForAppUpdates(
      "app1", &app1_update_status, &update_policy));
  EXPECT_TRUE(app1_update_status.effective_policy());
  const PolicyStatus<int>::Entry& app1_update_policy =
      app1_update_status.effective_policy().value();
  EXPECT_EQ(app1_update_policy.source, "device_management");
  EXPECT_EQ(app1_update_policy.policy, 3);
  EXPECT_TRUE(app1_update_status.conflict_policy());
  const PolicyStatus<int>::Entry& app1_update_conflict_policy =
      app1_update_status.conflict_policy().value();
  EXPECT_TRUE(app1_update_status.conflict_policy());
  EXPECT_EQ(app1_update_conflict_policy.policy, 2);
  EXPECT_EQ(app1_update_conflict_policy.source, "imaginary");
  EXPECT_EQ(update_policy, 3);

  PolicyStatus<int> app2_update_status;
  EXPECT_TRUE(policy_service->GetEffectivePolicyForAppUpdates(
      "app2", &app2_update_status, &update_policy));
  EXPECT_TRUE(app2_update_status.effective_policy());
  const PolicyStatus<int>::Entry& app2_update_policy =
      app2_update_status.effective_policy().value();
  EXPECT_EQ(app2_update_policy.source, "group_policy");
  EXPECT_EQ(app2_update_policy.policy, 1);
  EXPECT_EQ(update_policy, 1);
  EXPECT_FALSE(app2_update_status.conflict_policy());

  PolicyStatus<std::string> download_preference_status;
  std::string download_preference;
  EXPECT_TRUE(policy_service->GetDownloadPreferenceGroupPolicy(
      &download_preference_status, &download_preference));
  EXPECT_TRUE(download_preference_status.effective_policy());
  const PolicyStatus<std::string>::Entry& download_preference_policy =
      download_preference_status.effective_policy().value();
  EXPECT_EQ(download_preference_policy.source, "imaginary");
  EXPECT_EQ(download_preference_policy.policy, "cacheable");
  EXPECT_EQ(download_preference, "cacheable");
  EXPECT_FALSE(download_preference_status.conflict_policy());

  int cache_size_limit = 0;
  EXPECT_FALSE(policy_service->GetPackageCacheSizeLimitMBytes(
      nullptr, &cache_size_limit));
}

TEST(PolicyService, MultiplePolicyManagers_WithUnmanagedOnes) {
  std::unique_ptr<PolicyService> policy_service(GetUpdaterPolicyService());
  std::vector<std::unique_ptr<PolicyManagerInterface>> managers;

  auto manager = std::make_unique<FakePolicyManager>(true, "device_management");
  UpdatesSuppressedTimes updates_suppressed_times = {5, 10, 30};
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_dm");
  manager->SetUpdatePolicy("app1", 3);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>(true, "imaginary");
  updates_suppressed_times = {1, 1, 20};
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_imaginary");
  manager->SetUpdatePolicy("app1", 2);
  manager->SetDownloadPreferenceGroupPolicy("cacheable");
  managers.push_back(std::move(manager));

  // The default policy manager.
  managers.push_back(GetPolicyManager());

  manager = std::make_unique<FakePolicyManager>(false, "group_policy");
  updates_suppressed_times = {5, 10, 30};
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_gp");
  manager->SetUpdatePolicy("app2", 1);
  managers.push_back(std::move(manager));

  policy_service->SetPolicyManagersForTesting(std::move(managers));
  EXPECT_EQ(policy_service->source(), "device_management;imaginary;default");

  PolicyStatus<UpdatesSuppressedTimes> suppressed_time_status;
  EXPECT_TRUE(policy_service->GetUpdatesSuppressedTimes(
      &suppressed_time_status, &updates_suppressed_times));
  EXPECT_TRUE(suppressed_time_status.conflict_policy());
  EXPECT_EQ(suppressed_time_status.effective_policy().value().source,
            "device_management");
  EXPECT_EQ(updates_suppressed_times.start_hour, 5);
  EXPECT_EQ(updates_suppressed_times.start_minute, 10);
  EXPECT_EQ(updates_suppressed_times.duration_minute, 30);

  PolicyStatus<std::string> channel_status;
  std::string channel;
  EXPECT_TRUE(
      policy_service->GetTargetChannel("app1", &channel_status, &channel));
  EXPECT_TRUE(channel_status.effective_policy());
  const PolicyStatus<std::string>::Entry& channel_status_policy =
      channel_status.effective_policy().value();
  EXPECT_EQ(channel_status_policy.source, "device_management");
  EXPECT_EQ(channel_status_policy.policy, "channel_dm");
  EXPECT_TRUE(channel_status.conflict_policy());
  const PolicyStatus<std::string>::Entry& channel_status_conflict_policy =
      channel_status.conflict_policy().value();
  EXPECT_EQ(channel_status_conflict_policy.policy, "channel_imaginary");
  EXPECT_EQ(channel_status_conflict_policy.source, "imaginary");
  EXPECT_EQ(channel, "channel_dm");

  PolicyStatus<int> app1_update_status;
  int update_policy = 0;
  EXPECT_TRUE(policy_service->GetEffectivePolicyForAppUpdates(
      "app1", &app1_update_status, &update_policy));
  const PolicyStatus<int>::Entry& app1_update_status_policy =
      app1_update_status.effective_policy().value();
  EXPECT_EQ(app1_update_status_policy.source, "device_management");
  EXPECT_EQ(app1_update_status_policy.policy, 3);
  EXPECT_TRUE(app1_update_status.conflict_policy());
  const PolicyStatus<int>::Entry& app1_update_status_conflict_policy =
      app1_update_status.conflict_policy().value();
  EXPECT_EQ(app1_update_status_conflict_policy.source, "imaginary");
  EXPECT_EQ(app1_update_status_conflict_policy.policy, 2);
  EXPECT_EQ(update_policy, 3);

  PolicyStatus<int> app2_update_status;
  EXPECT_FALSE(policy_service->GetEffectivePolicyForAppUpdates(
      "app2", &app2_update_status, &update_policy));
  EXPECT_FALSE(app2_update_status.effective_policy());
  EXPECT_FALSE(app2_update_status.conflict_policy());

  PolicyStatus<std::string> download_preference_status;
  std::string download_preference;
  EXPECT_TRUE(policy_service->GetDownloadPreferenceGroupPolicy(
      &download_preference_status, &download_preference));
  EXPECT_TRUE(download_preference_status.effective_policy());
  EXPECT_EQ(download_preference_status.effective_policy().value().source,
            "imaginary");
  EXPECT_EQ(download_preference, "cacheable");
  EXPECT_FALSE(download_preference_status.conflict_policy());

  int cache_size_limit = 0;
  EXPECT_FALSE(policy_service->GetPackageCacheSizeLimitMBytes(
      nullptr, &cache_size_limit));
}

}  // namespace updater
