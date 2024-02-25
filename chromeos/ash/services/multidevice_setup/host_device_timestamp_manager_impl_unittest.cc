// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager_impl.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {
const base::Time kTestTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1500000000000);
const base::Time kLaterTime = kTestTime + base::Milliseconds(123456789);
}  // namespace

class HostDeviceTimestampManagerImplTest : public testing::Test {
 public:
  HostDeviceTimestampManagerImplTest(
      const HostDeviceTimestampManagerImplTest&) = delete;
  HostDeviceTimestampManagerImplTest& operator=(
      const HostDeviceTimestampManagerImplTest&) = delete;

 protected:
  HostDeviceTimestampManagerImplTest() = default;
  ~HostDeviceTimestampManagerImplTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    HostDeviceTimestampManagerImpl::RegisterPrefs(
        test_pref_service_->registry());

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    SetNow(kTestTime);

    manager_ = HostDeviceTimestampManagerImpl::Factory::Create(
        fake_host_status_provider_.get(), test_pref_service_.get(),
        test_clock_.get());
  }

  // If the status corresponds to a set host, we set a dummy default device
  // because the device info is irrelevant.
  void SetHostStatus(mojom::HostStatus host_status) {
    if (host_status == mojom::HostStatus::kNoEligibleHosts ||
        host_status == mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
      fake_host_status_provider_->SetHostWithStatus(
          host_status, std::nullopt /* host_device */);
      return;
    }
    fake_host_status_provider_->SetHostWithStatus(
        host_status, multidevice::RemoteDeviceRefBuilder()
                         .SetPublicKey("fake-phone-key")
                         .SetName("Fake Phone Name")
                         .Build());
  }

  void SetNow(const base::Time now) { test_clock_->SetNow(now); }

  HostDeviceTimestampManager* manager() { return manager_.get(); }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<HostDeviceTimestampManager> manager_;
};

TEST_F(HostDeviceTimestampManagerImplTest,
       RecordsWhetherHostWasSetFromThisChromebook) {
  EXPECT_FALSE(manager()->WasHostSetFromThisChromebook());
  // Discover potential host.
  SetHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet);

  // Set up host.
  SetHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  EXPECT_TRUE(manager()->WasHostSetFromThisChromebook());

  // Verify host.
  SetHostStatus(mojom::HostStatus::kHostVerified);
  EXPECT_TRUE(manager()->WasHostSetFromThisChromebook());

  // Forget the host.
  SetHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet);
  EXPECT_FALSE(manager()->WasHostSetFromThisChromebook());

  // Set verified host from a different Chromebook
  SetHostStatus(mojom::HostStatus::kHostVerified);
  EXPECT_FALSE(manager()->WasHostSetFromThisChromebook());
}

TEST_F(HostDeviceTimestampManagerImplTest, RecordsCorrectCompletionTime) {
  EXPECT_FALSE(manager()->GetLatestSetupFlowCompletionTimestamp());
  SetHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  EXPECT_EQ(kTestTime, manager()->GetLatestSetupFlowCompletionTimestamp());
  // Forget the host.
  SetHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet);
  // Set up later and check that the new time replaces the old.
  SetNow(kLaterTime);
  SetHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  EXPECT_EQ(kLaterTime, manager()->GetLatestSetupFlowCompletionTimestamp());
}

TEST_F(HostDeviceTimestampManagerImplTest, RecordsCorrectVerificationTime) {
  EXPECT_FALSE(manager()->GetLatestVerificationTimestamp());
  // Update with verified host.
  SetHostStatus(mojom::HostStatus::kHostVerified);
  EXPECT_EQ(kTestTime, manager()->GetLatestVerificationTimestamp());
  // Forget the host.
  SetHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet);
  // Set up later and check that the new time replaces the old.
  SetNow(kLaterTime);
  SetHostStatus(mojom::HostStatus::kHostVerified);
  EXPECT_EQ(kLaterTime, manager()->GetLatestVerificationTimestamp());
}

TEST_F(HostDeviceTimestampManagerImplTest,
       StoresCompletionsAndVerificationSimultaneously) {
  SetHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation);
  SetNow(kLaterTime);
  SetHostStatus(mojom::HostStatus::kHostVerified);
  EXPECT_EQ(kTestTime, manager()->GetLatestSetupFlowCompletionTimestamp());
  EXPECT_EQ(kLaterTime, manager()->GetLatestVerificationTimestamp());
}
}  // namespace multidevice_setup

}  // namespace ash
