// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// The Policy Manager Interface is implemented by policy managers such as Group
// Policy and Device Management.
class FakePolicyManager : public PolicyManagerInterface {
 public:
  FakePolicyManager(bool has_active_device_policies, const std::string& source)
      : has_active_device_policies_(has_active_device_policies),
        source_(source) {}
  ~FakePolicyManager() override = default;

  std::string source() const override { return source_; }
  bool HasActiveDevicePolicies() const override {
    return has_active_device_policies_;
  }
  absl::optional<base::TimeDelta> GetLastCheckPeriod() const override {
    return absl::nullopt;
  }
  absl::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override {
    if (!suppressed_times_.valid())
      return absl::nullopt;

    return suppressed_times_;
  }
  void SetUpdatesSuppressedTimes(
      const UpdatesSuppressedTimes& suppressed_times) {
    suppressed_times_ = suppressed_times;
  }
  absl::optional<std::string> GetDownloadPreferenceGroupPolicy()
      const override {
    if (download_preference_.empty())
      return absl::nullopt;

    return download_preference_;
  }
  void SetDownloadPreferenceGroupPolicy(const std::string& preference) {
    download_preference_ = preference;
  }
  absl::optional<int> GetPackageCacheSizeLimitMBytes() const override {
    return absl::nullopt;
  }
  absl::optional<int> GetPackageCacheExpirationTimeDays() const override {
    return absl::nullopt;
  }
  absl::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override {
    return absl::nullopt;
  }
  absl::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override {
    auto value = update_policies_.find(app_id);
    if (value == update_policies_.end())
      return absl::nullopt;
    return value->second;
  }
  void SetUpdatePolicy(const std::string& app_id, int update_policy) {
    update_policies_[app_id] = update_policy;
  }
  absl::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override {
    return absl::nullopt;
  }
  absl::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override {
    return absl::nullopt;
  }
  absl::optional<std::string> GetProxyMode() const override {
    return absl::nullopt;
  }
  absl::optional<std::string> GetProxyPacUrl() const override {
    return absl::nullopt;
  }
  absl::optional<std::string> GetProxyServer() const override {
    return absl::nullopt;
  }
  absl::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override {
    auto value = channels_.find(app_id);
    if (value == channels_.end())
      return absl::nullopt;
    return value->second;
  }
  void SetChannel(const std::string& app_id, std::string channel) {
    channels_[app_id] = std::move(channel);
  }
  absl::optional<std::vector<std::string>> GetForceInstallApps()
      const override {
    return absl::nullopt;
  }

 private:
  bool has_active_device_policies_;
  std::string source_;
  UpdatesSuppressedTimes suppressed_times_;
  std::string download_preference_;
  std::map<std::string, std::string> channels_;
  std::map<std::string, int> update_policies_;
};

TEST(PolicyService, DefaultPolicyValue) {
  PolicyService::PolicyManagerVector managers;
  managers.push_back(GetDefaultValuesPolicyManager());
  auto policy_service =
      base::MakeRefCounted<PolicyService>(std::move(managers));
  EXPECT_EQ(policy_service->source(), "default");

  PolicyStatus<std::string> version_prefix =
      policy_service->GetTargetVersionPrefix("");
  EXPECT_FALSE(version_prefix);

  PolicyStatus<base::TimeDelta> last_check =
      policy_service->GetLastCheckPeriod();
  ASSERT_TRUE(last_check);
  EXPECT_EQ(last_check.policy(), base::Minutes(270));

  PolicyStatus<int> app_installs =
      policy_service->GetPolicyForAppInstalls("test1");
  ASSERT_TRUE(app_installs);
  EXPECT_EQ(app_installs.policy(), 1);

  PolicyStatus<int> app_updates =
      policy_service->GetPolicyForAppUpdates("test1");
  ASSERT_TRUE(app_updates);
  EXPECT_EQ(app_updates.policy(), 1);

  PolicyStatus<bool> rollback_allowed =
      policy_service->IsRollbackToTargetVersionAllowed("test1");
  ASSERT_TRUE(rollback_allowed);
  EXPECT_EQ(rollback_allowed.policy(), false);
}

TEST(PolicyService, SinglePolicyManager) {
  auto manager = std::make_unique<FakePolicyManager>(true, "test_source");
  manager->SetChannel("app1", "test_channel");
  manager->SetUpdatePolicy("app2", 3);
  PolicyService::PolicyManagerVector managers;
  managers.push_back(std::move(manager));
  auto policy_service =
      base::MakeRefCounted<PolicyService>(std::move(managers));
  EXPECT_EQ(policy_service->source(), "test_source");

  PolicyStatus<std::string> app1_channel =
      policy_service->GetTargetChannel("app1");
  ASSERT_TRUE(app1_channel);
  EXPECT_EQ(app1_channel.policy(), "test_channel");
  EXPECT_EQ(app1_channel.conflict_policy(), absl::nullopt);

  PolicyStatus<std::string> app2_channel =
      policy_service->GetTargetChannel("app2");
  EXPECT_FALSE(app2_channel);
  EXPECT_EQ(app2_channel.conflict_policy(), absl::nullopt);

  PolicyStatus<int> app1_update_status =
      policy_service->GetPolicyForAppUpdates("app1");
  EXPECT_FALSE(app1_update_status);
  EXPECT_EQ(app1_update_status.conflict_policy(), absl::nullopt);

  PolicyStatus<int> app2_update_status =
      policy_service->GetPolicyForAppUpdates("app2");
  EXPECT_TRUE(app2_update_status);
  EXPECT_EQ(app2_update_status.policy(), 3);
  EXPECT_EQ(app2_update_status.conflict_policy(), absl::nullopt);
}

TEST(PolicyService, MultiplePolicyManagers) {
  PolicyService::PolicyManagerVector managers;

  auto manager = std::make_unique<FakePolicyManager>(true, "group_policy");
  UpdatesSuppressedTimes updates_suppressed_times;
  updates_suppressed_times.start_hour_ = 5;
  updates_suppressed_times.start_minute_ = 10;
  updates_suppressed_times.duration_minute_ = 30;
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
  updates_suppressed_times.start_hour_ = 1;
  updates_suppressed_times.start_minute_ = 1;
  updates_suppressed_times.duration_minute_ = 20;
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_imaginary");
  manager->SetUpdatePolicy("app1", 2);
  manager->SetDownloadPreferenceGroupPolicy("cacheable");
  managers.push_back(std::move(manager));

  // The default policy manager.
  managers.push_back(GetDefaultValuesPolicyManager());

  auto policy_service =
      base::MakeRefCounted<PolicyService>(std::move(managers));
  EXPECT_EQ(policy_service->source(),
            "group_policy;device_management;imaginary;default");

  PolicyStatus<UpdatesSuppressedTimes> suppressed_time_status =
      policy_service->GetUpdatesSuppressedTimes();
  ASSERT_TRUE(suppressed_time_status);
  EXPECT_TRUE(suppressed_time_status.conflict_policy());
  EXPECT_EQ(suppressed_time_status.effective_policy().value().source,
            "group_policy");
  EXPECT_EQ(suppressed_time_status.policy().start_hour_, 5);
  EXPECT_EQ(suppressed_time_status.policy().start_minute_, 10);
  EXPECT_EQ(suppressed_time_status.policy().duration_minute_, 30);

  PolicyStatus<std::string> channel_status =
      policy_service->GetTargetChannel("app1");
  ASSERT_TRUE(channel_status);
  const PolicyStatus<std::string>::Entry& channel_policy =
      channel_status.effective_policy().value();
  EXPECT_EQ(channel_policy.source, "group_policy");
  EXPECT_EQ(channel_policy.policy, "channel_gp");
  EXPECT_TRUE(channel_status.conflict_policy());
  const PolicyStatus<std::string>::Entry& channel_conflict_policy =
      channel_status.conflict_policy().value();
  EXPECT_EQ(channel_conflict_policy.source, "device_management");
  EXPECT_EQ(channel_conflict_policy.policy, "channel_dm");
  EXPECT_EQ(channel_status.policy(), "channel_gp");

  PolicyStatus<int> app1_update_status =
      policy_service->GetPolicyForAppUpdates("app1");
  ASSERT_TRUE(app1_update_status);
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
  EXPECT_EQ(app1_update_status.policy(), 3);

  PolicyStatus<int> app2_update_status =
      policy_service->GetPolicyForAppUpdates("app2");
  ASSERT_TRUE(app2_update_status);
  const PolicyStatus<int>::Entry& app2_update_policy =
      app2_update_status.effective_policy().value();
  EXPECT_EQ(app2_update_policy.source, "group_policy");
  EXPECT_EQ(app2_update_policy.policy, 1);
  EXPECT_EQ(app2_update_status.policy(), 1);
  EXPECT_EQ(app2_update_status.conflict_policy(), absl::nullopt);

  PolicyStatus<std::string> download_preference_status =
      policy_service->GetDownloadPreferenceGroupPolicy();
  ASSERT_TRUE(download_preference_status);
  const PolicyStatus<std::string>::Entry& download_preference_policy =
      download_preference_status.effective_policy().value();
  EXPECT_EQ(download_preference_policy.source, "imaginary");
  EXPECT_EQ(download_preference_policy.policy, "cacheable");
  EXPECT_EQ(download_preference_status.policy(), "cacheable");
  EXPECT_EQ(download_preference_status.conflict_policy(), absl::nullopt);

  EXPECT_FALSE(policy_service->GetPackageCacheSizeLimitMBytes());
}

TEST(PolicyService, MultiplePolicyManagers_WithUnmanagedOnes) {
  PolicyService::PolicyManagerVector managers;

  auto manager = std::make_unique<FakePolicyManager>(true, "device_management");
  UpdatesSuppressedTimes updates_suppressed_times;
  updates_suppressed_times.start_hour_ = 5;
  updates_suppressed_times.start_minute_ = 10;
  updates_suppressed_times.duration_minute_ = 30;
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_dm");
  manager->SetUpdatePolicy("app1", 3);
  managers.push_back(std::move(manager));

  manager = std::make_unique<FakePolicyManager>(true, "imaginary");
  updates_suppressed_times.start_hour_ = 1;
  updates_suppressed_times.start_minute_ = 1;
  updates_suppressed_times.duration_minute_ = 20;
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_imaginary");
  manager->SetUpdatePolicy("app1", 2);
  manager->SetDownloadPreferenceGroupPolicy("cacheable");
  managers.push_back(std::move(manager));

  managers.push_back(GetDefaultValuesPolicyManager());

  manager = std::make_unique<FakePolicyManager>(false, "group_policy");
  updates_suppressed_times.start_hour_ = 5;
  updates_suppressed_times.start_minute_ = 10;
  updates_suppressed_times.duration_minute_ = 30;
  manager->SetUpdatesSuppressedTimes(updates_suppressed_times);
  manager->SetChannel("app1", "channel_gp");
  manager->SetUpdatePolicy("app2", 1);
  managers.push_back(std::move(manager));

  auto policy_service =
      base::MakeRefCounted<PolicyService>(std::move(managers));
  EXPECT_EQ(policy_service->source(), "device_management;imaginary;default");

  PolicyStatus<UpdatesSuppressedTimes> suppressed_time_status =
      policy_service->GetUpdatesSuppressedTimes();
  ASSERT_TRUE(suppressed_time_status);
  EXPECT_EQ(suppressed_time_status.effective_policy().value().source,
            "device_management");
  EXPECT_EQ(suppressed_time_status.policy().start_hour_, 5);
  EXPECT_EQ(suppressed_time_status.policy().start_minute_, 10);
  EXPECT_EQ(suppressed_time_status.policy().duration_minute_, 30);

  PolicyStatus<std::string> channel_status =
      policy_service->GetTargetChannel("app1");
  ASSERT_TRUE(channel_status);
  const PolicyStatus<std::string>::Entry& channel_status_policy =
      channel_status.effective_policy().value();
  EXPECT_EQ(channel_status_policy.source, "device_management");
  EXPECT_EQ(channel_status_policy.policy, "channel_dm");
  EXPECT_TRUE(channel_status.conflict_policy());
  const PolicyStatus<std::string>::Entry& channel_status_conflict_policy =
      channel_status.conflict_policy().value();
  EXPECT_EQ(channel_status_conflict_policy.policy, "channel_imaginary");
  EXPECT_EQ(channel_status_conflict_policy.source, "imaginary");
  EXPECT_EQ(channel_status.policy(), "channel_dm");

  PolicyStatus<int> app1_update_status =
      policy_service->GetPolicyForAppUpdates("app1");
  ASSERT_TRUE(app1_update_status);
  const PolicyStatus<int>::Entry& app1_update_status_policy =
      app1_update_status.effective_policy().value();
  EXPECT_EQ(app1_update_status_policy.source, "device_management");
  EXPECT_EQ(app1_update_status_policy.policy, 3);
  EXPECT_TRUE(app1_update_status.conflict_policy());
  const PolicyStatus<int>::Entry& app1_update_status_conflict_policy =
      app1_update_status.conflict_policy().value();
  EXPECT_EQ(app1_update_status_conflict_policy.source, "imaginary");
  EXPECT_EQ(app1_update_status_conflict_policy.policy, 2);
  EXPECT_EQ(app1_update_status.policy(), 3);

  PolicyStatus<int> app2_update_status =
      policy_service->GetPolicyForAppUpdates("app2");
  ASSERT_TRUE(app2_update_status);
  EXPECT_EQ(app2_update_status.conflict_policy(), absl::nullopt);
  const PolicyStatus<int>::Entry& app2_update_status_policy =
      app2_update_status.effective_policy().value();
  EXPECT_EQ(app2_update_status_policy.source, "default");
  EXPECT_EQ(app2_update_status_policy.policy, 1);
  EXPECT_EQ(app2_update_status.policy(), 1);

  PolicyStatus<std::string> download_preference_status =
      policy_service->GetDownloadPreferenceGroupPolicy();
  ASSERT_TRUE(download_preference_status);
  EXPECT_EQ(download_preference_status.effective_policy().value().source,
            "imaginary");
  EXPECT_EQ(download_preference_status.policy(), "cacheable");
  EXPECT_EQ(download_preference_status.conflict_policy(), absl::nullopt);

  EXPECT_FALSE(policy_service->GetPackageCacheSizeLimitMBytes());
}

}  // namespace updater
