// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kDeviceIdValue[] = "test-device-id-value";
constexpr char kEnrollmentTokenValue[] = "test-enrollment-token-value";
constexpr char kLastReportTimeKey[] = "lastCloudReportSentTimestamp";
constexpr char kReportTimestampPref[] = "enterprise.last_report_timestamp";
constexpr char kStatusKey[] = "status";
constexpr char kTimeSinceReportKey[] = "timeSinceLastCloudReportSent";

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
constexpr char kExpectedDescriptionKey[] = "statusDevice";
#else
constexpr char kExpectedDescriptionKey[] = "statusMachine";
#endif

class TestPolicyStatusProviderObserver : public PolicyStatusProvider::Observer {
 public:
  TestPolicyStatusProviderObserver() = default;
  ~TestPolicyStatusProviderObserver() override = default;

  void OnPolicyStatusChanged() override { status_changed_count_++; }
  int status_changed_count() const { return status_changed_count_; }

 private:
  int status_changed_count_ = 0;
};

}  // namespace

class MachineLevelUserCloudPolicyStatusProviderTest : public testing::Test {
 public:
  MachineLevelUserCloudPolicyStatusProviderTest() = default;
  ~MachineLevelUserCloudPolicyStatusProviderTest() override = default;

  void SetUp() override {
    prefs_.registry()->RegisterTimePref(kReportTimestampPref, base::Time());
    core_ = std::make_unique<CloudPolicyCore>(
        dm_protocol::GetChromeUserPolicyType(), std::string(), &store_,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
    context_.enrollmentToken = kEnrollmentTokenValue;
    context_.deviceId = kDeviceIdValue;
    context_.lastReportTimestampPrefName = kReportTimestampPref;
  }

  void TearDown() override { core_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockCloudPolicyStore store_{dm_protocol::GetChromeUserPolicyType()};
  TestingPrefServiceSimple prefs_;
  MachineLevelUserCloudPolicyContext context_;
  std::unique_ptr<CloudPolicyCore> core_;
};

TEST_F(MachineLevelUserCloudPolicyStatusProviderTest, GetStatusComplete) {
  prefs_.SetTime(kReportTimestampPref, base::Time::Now());

  MachineLevelUserCloudPolicyStatusProvider provider(
      core_.get(), /*extension_install_core=*/nullptr, &prefs_, &context_);

  base::DictValue status = provider.GetStatus();

  const std::string* device_id = status.FindString(kDeviceIdKey);
  ASSERT_TRUE(device_id);
  EXPECT_EQ(kDeviceIdValue, *device_id);

  const std::string* token = status.FindString(kEnrollmentTokenKey);
  ASSERT_TRUE(token);
  EXPECT_EQ(kEnrollmentTokenValue, *token);

  const std::string* desc = status.FindString(kPolicyDescriptionKey);
  ASSERT_TRUE(desc);
  EXPECT_EQ(kExpectedDescriptionKey, *desc);

  const std::string* machine = status.FindString(kMachineKey);
  ASSERT_TRUE(machine);
#if BUILDFLAG(IS_ANDROID)
  // On Android, GetMachineName() always returns an empty string.
  EXPECT_TRUE(machine->empty());
#else
  EXPECT_FALSE(machine->empty());
#endif

  const std::string* last_report = status.FindString(kLastReportTimeKey);
  ASSERT_TRUE(last_report);
  EXPECT_FALSE(last_report->empty());

  const std::string* time_since = status.FindString(kTimeSinceReportKey);
  ASSERT_TRUE(time_since);
  EXPECT_FALSE(time_since->empty());

  EXPECT_TRUE(status.FindString(kStatusKey));
}

TEST_F(MachineLevelUserCloudPolicyStatusProviderTest, GetStatusMojoComplete) {
  prefs_.SetTime(kReportTimestampPref, base::Time::Now());

  MachineLevelUserCloudPolicyStatusProvider provider(
      core_.get(), /*extension_install_core=*/nullptr, &prefs_, &context_);

  policy::mojom::StatusPtr status = provider.GetStatusMojo();
  ASSERT_TRUE(status);

  EXPECT_EQ(kDeviceIdValue, status->device_id);
  EXPECT_EQ(kEnrollmentTokenValue, status->enrollment_token);
  EXPECT_EQ(kExpectedDescriptionKey, status->policy_description_key);
  ASSERT_TRUE(status->machine.has_value());
#if BUILDFLAG(IS_ANDROID)
  // On Android, GetMachineName() always returns an empty string.
  EXPECT_TRUE(status->machine->empty());
#else
  EXPECT_FALSE(status->machine->empty());
#endif
  ASSERT_TRUE(status->last_cloud_report_sent_timestamp.has_value());
  EXPECT_FALSE(status->last_cloud_report_sent_timestamp->empty());
  EXPECT_FALSE(status->status.empty());
}

TEST_F(MachineLevelUserCloudPolicyStatusProviderTest, ObserverNotification) {
  MachineLevelUserCloudPolicyStatusProvider provider(
      core_.get(), /*extension_install_core=*/nullptr, &prefs_, &context_);
  TestPolicyStatusProviderObserver observer;
  provider.AddObserver(&observer);

  EXPECT_EQ(0, observer.status_changed_count());

  provider.OnStoreLoaded(&store_);
  EXPECT_EQ(1, observer.status_changed_count());

  provider.OnStoreError(&store_);
  EXPECT_EQ(2, observer.status_changed_count());

  provider.RemoveObserver(&observer);
}

}  // namespace policy
