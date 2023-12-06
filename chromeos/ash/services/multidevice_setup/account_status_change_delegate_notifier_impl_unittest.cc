// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_device_timestamp_manager.h"
#include "chromeos/ash/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const int64_t kTestTimeMillis = 1500000000000;
const char kFakePhoneKey[] = "fake-phone-key";
const char kFakePhoneName[] = "Phony Phone";
const char kFakePhoneKeyA[] = "fake-phone-key-A";
const char kFakePhoneNameA[] = "Phony Phone A";
const char kFakePhoneKeyB[] = "fake-phone-key-B";
const char kFakePhoneNameB[] = "Phony Phone B";

const multidevice::RemoteDeviceRef kFakePhone =
    multidevice::RemoteDeviceRefBuilder()
        .SetPublicKey(kFakePhoneKey)
        .SetName(kFakePhoneName)
        .Build();

// Alternate hosts for multi host tests
const multidevice::RemoteDeviceRef kFakePhoneA =
    multidevice::RemoteDeviceRefBuilder()
        .SetPublicKey("fake-phone-key-A")
        .SetName("Phony Phone A")
        .Build();
const multidevice::RemoteDeviceRef kFakePhoneB =
    multidevice::RemoteDeviceRefBuilder()
        .SetPublicKey("fake-phone-key-B")
        .SetName("Phony Phone B")
        .Build();

}  // namespace

class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest
    : public testing::Test {
 public:
  MultiDeviceSetupAccountStatusChangeDelegateNotifierTest(
      const MultiDeviceSetupAccountStatusChangeDelegateNotifierTest&) = delete;
  MultiDeviceSetupAccountStatusChangeDelegateNotifierTest& operator=(
      const MultiDeviceSetupAccountStatusChangeDelegateNotifierTest&) = delete;

 protected:
  MultiDeviceSetupAccountStatusChangeDelegateNotifierTest() = default;

  ~MultiDeviceSetupAccountStatusChangeDelegateNotifierTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(
        test_pref_service_->registry());

    fake_delegate_ = std::make_unique<FakeAccountStatusChangeDelegate>();
    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    fake_host_device_timestamp_manager_ =
        std::make_unique<FakeHostDeviceTimestampManager>();
    fake_oobe_completion_tracker_ = std::make_unique<OobeCompletionTracker>();
    test_clock_->SetNow(
        base::Time::FromMillisecondsSinceUnixEpoch(kTestTimeMillis));
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void BuildAccountStatusChangeDelegateNotifier() {
    delegate_notifier_ =
        AccountStatusChangeDelegateNotifierImpl::Factory::Create(
            fake_host_status_provider_.get(), test_pref_service_.get(),
            fake_host_device_timestamp_manager_.get(),
            fake_oobe_completion_tracker_.get(), test_clock_.get());
  }

  multidevice::RemoteDeviceRef BuildFakePhone(std::string public_key,
                                              std::string name) {
    return multidevice::RemoteDeviceRefBuilder()
        .SetPublicKey(public_key)
        .SetName(name)
        .Build();
  }

  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
    delegate_notifier_->FlushForTesting();
  }

  // Simulates finding a potential host, going through setup flow with it, and
  // verifying it.
  void SetUpHost(const multidevice::RemoteDeviceRef& host_device) {
    SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                      std::nullopt /* host_device */);
    fake_host_device_timestamp_manager_->set_was_host_set_from_this_chromebook(
        true);
    SetHostWithStatus(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        host_device);
    SetHostWithStatus(mojom::HostStatus::kHostVerified, host_device);
  }

  // Simulates forgetting a set host.
  void ForgetHost() {
    fake_host_device_timestamp_manager_->set_was_host_set_from_this_chromebook(
        false);
    SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                      std::nullopt /* host_device */);
  }

  void SetNewUserPotentialHostExistsTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kNewUserPotentialHostExistsPrefName,
                                 timestamp);
  }

  void SetExistingUserChromebookAddedTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kExistingUserChromebookAddedPrefName,
                                 timestamp);
  }

  void SetHostFromPreviousSession(std::string old_host_key) {
    std::string old_host_device_id =
        multidevice::RemoteDevice::GenerateDeviceId(old_host_key);
    test_pref_service_->SetString(
        AccountStatusChangeDelegateNotifierImpl::
            kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName,
        old_host_device_id);
  }

  void SetAccountStatusChangeDelegateRemote() {
    delegate_notifier_->SetAccountStatusChangeDelegateRemote(
        fake_delegate_->GenerateRemote());
    delegate_notifier_->FlushForTesting();
  }

  // Simulates completing setup flow in OOBE.
  void CompleteOobeSetupFlow() {
    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        std::nullopt /* host_device */);
    fake_oobe_completion_tracker_->MarkOobeShown();
    if (fake_delegate_)
      delegate_notifier_->FlushForTesting();
  }

  int64_t GetNewUserPotentialHostExistsTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kNewUserPotentialHostExistsPrefName);
  }

  int64_t GetExistingUserChromebookAddedTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kExistingUserChromebookAddedPrefName);
  }

  int64_t GetOobeSetupFlowTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kOobeSetupFlowTimestampPrefName);
  }

  std::string GetMostRecentVerifiedHostDeviceIdPref() {
    return test_pref_service_->GetString(
        AccountStatusChangeDelegateNotifierImpl::
            kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName);
  }

  void FastForward(int64_t time_delta) {
    test_clock_->SetNow(base::Time::FromMillisecondsSinceUnixEpoch(
        kTestTimeMillis + time_delta));
    if (fake_delegate_) {
      delegate_notifier_->FlushForTesting();
    }
  }

  void SetSeeionState(session_manager::SessionState state) {
    session_manager_->SetSessionState(state);
    if (fake_delegate_) {
      delegate_notifier_->FlushForTesting();
    }
  }

  FakeAccountStatusChangeDelegate* fake_delegate() {
    return fake_delegate_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeAccountStatusChangeDelegate> fake_delegate_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostDeviceTimestampManager>
      fake_host_device_timestamp_manager_;
  std::unique_ptr<OobeCompletionTracker> fake_oobe_completion_tracker_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;
};

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SetObserverWithPotentialHost) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  // We are now showing nudge instead of notification.
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       PotentialHostAddedLater) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);

  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  // We are now showing nudge instead of notification.
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OnlyPotentialHostCausesNewUserEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoNewUserEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // All conditions for new user event are satisfied except for setting a
  // delegate.
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);

  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NewUserFlowBlockedByOldNewUserTimestamp) {
  BuildAccountStatusChangeDelegateNotifier();
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetNewUserPotentialHostExistsTimestamp(earlier_test_time_millis);
  SetAccountStatusChangeDelegateRemote();

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  // Timestamp was not overwritten by clock.
  EXPECT_EQ(earlier_test_time_millis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NewUserFlowBlockedByOldChromebookAddedTimestamp) {
  BuildAccountStatusChangeDelegateNotifier();
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);
  SetAccountStatusChangeDelegateRemote();

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  // Timestamp was not overwritten by clock.
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       LosingPotentialHostTriggersNoLongerNewUserEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, fake_delegate()->num_no_longer_new_user_events_handled());

  // All potential hosts are lost from the account.
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(1u, fake_delegate()->num_no_longer_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SettingHostTriggersNoLongerNewUserEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, fake_delegate()->num_no_longer_new_user_events_handled());

  // A potential host was set.
  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(1u, fake_delegate()->num_no_longer_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       CompletingOobeSetupFlowBlocksNewUserEventIfNoDelegateIsSet) {
  BuildAccountStatusChangeDelegateNotifier();

  // Complete OOBE MultiDevice setup flow before delegate is set.
  EXPECT_EQ(0u, GetOobeSetupFlowTimestamp());
  CompleteOobeSetupFlow();
  EXPECT_EQ(kTestTimeMillis, GetOobeSetupFlowTimestamp());

  // Set delegate, which triggers event check.
  SetAccountStatusChangeDelegateRemote();

  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       CompletingOobeSetupFlowWithDelegateSetTriggersNoLongerNewUserEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);

  // Complete OOBE MultiDevice setup flow before delegate is set.
  EXPECT_EQ(0u, GetOobeSetupFlowTimestamp());
  CompleteOobeSetupFlow();
  EXPECT_EQ(kTestTimeMillis, GetOobeSetupFlowTimestamp());

  EXPECT_EQ(0u, fake_delegate()->num_new_user_potential_host_events_handled());
  EXPECT_EQ(1u, fake_delegate()->num_no_longer_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoLongerNewUserEventBlockedByOldChromebookAddedTimestamp) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();

  // Record earlier Chromebook added event.
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);

  // A potential host was found.
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);

  // A potential host was set. Note that this would trigger a NoLongerNewUser
  // event in the absence of the Chromebook added timestamp.
  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u, fake_delegate()->num_no_longer_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForHostSwitchEvents) {
  const multidevice::RemoteDeviceRef fakePhone =
      BuildFakePhone(kFakePhoneKey, kFakePhoneName);

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Check the delegate initializes to 0.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  // Host was set but has never been switched.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch to new verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA));
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch to a different new verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyB, kFakePhoneNameB));
  EXPECT_EQ(2u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch back to initial host (verified).
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  EXPECT_EQ(3u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SettingSameHostTriggersNoHostSwitchedEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  // Set to host with identical information.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChangingHostDevicesTriggersHostSwitchEventWhenHostNameIsUnchanged) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  // Set to verified host with same name but different key.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneName));
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       VerifyingSetHostTriggersNoHostSwtichEvent) {
  const multidevice::RemoteDeviceRef fakePhone =
      BuildFakePhone(kFakePhoneKey, kFakePhoneName);

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Set initial host but do not verify.
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified, fakePhone);
  // Verify host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OnlyVerifiedHostCausesHostSwitchedEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));

  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Set a new host without verifying.
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Set a different new host without confirming with backend so host is
  // unverified.
  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      BuildFakePhone(kFakePhoneKeyB, kFakePhoneNameB));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ForgettingAndThenSwitchingHostsDoesNotTriggerHostSwitchedEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));

  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  ForgetHost();
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Set a new verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       HostSwitchedBetweenSessions) {
  // Host set in some previous session.
  SetHostFromPreviousSession(
      BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA).GetDeviceId());
  BuildAccountStatusChangeDelegateNotifier();
  // Host switched and verified between sessions.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  SetAccountStatusChangeDelegateRemote();
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutExistingHost) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  SetUpHost(BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  // All conditions for host switched event are satisfied except for setting a
  // delegate.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA));
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForChromebookAddedEvents) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Check the delegate initializes to 0.
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  // Host is set and verified from another Chromebook while this one is logged
  // in.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OnlyVerifiedHostCausesChromebookAddedEvent) {
  const multidevice::RemoteDeviceRef fakePhone =
      BuildFakePhone(kFakePhoneKey, kFakePhoneName);

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Start with potential hosts but none set.
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    std::nullopt /* host_device */);

  // Set a host without verifying.
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified, fakePhone);
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  // Verify the new host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ReplacingUnverifiedHostAWithVerifiedHostBCausesChromebookAddedEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Start with potential hosts but none set.
  // Set initial host but do not verify.
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                    BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA));

  // Replace unverified Phone A with verified Phone B.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKeyB, kFakePhoneNameB));
  // This causes a 'Chromebook added' event.
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  // It does not cause a 'host switched' event.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChromebookAddedBetweenSessionsTriggersEvents) {
  BuildAccountStatusChangeDelegateNotifier();
  // Host is set and verified before this Chromebook is logged in.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));

  SetAccountStatusChangeDelegateRemote();
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChromebookAddedEventBlockedIfHostWasSetFromThisChromebook) {
  const multidevice::RemoteDeviceRef fakePhone =
      BuildFakePhone(kFakePhoneKey, kFakePhoneName);

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();

  SetUpHost(fakePhone);
  // The host was set on this Chromebook so it should not trigger the Chromebook
  // added event
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  ForgetHost();
  // Set verified host on another Chromebook.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OldTimestampInPreferencesDoesNotPreventChromebookAddedEvent) {
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);
  // Check timestamp was written.
  EXPECT_EQ(earlier_test_time_millis,
            GetExistingUserChromebookAddedTimestamp());

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();

  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  // Timestamp was overwritten by clock.
  EXPECT_EQ(kTestTimeMillis, GetExistingUserChromebookAddedTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoChromebookAddedEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // Triggers event check. Note that all conditions for Chromebook added event
  // are satisfied except for setting a delegate.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    BuildFakePhone(kFakePhoneKey, kFakePhoneName));
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       VerifiedHostIdStaysUpToDateInPrefs) {
  const multidevice::RemoteDeviceRef fakePhone =
      BuildFakePhone(kFakePhoneKey, kFakePhoneName);
  const multidevice::RemoteDeviceRef fakePhoneA =
      BuildFakePhone(kFakePhoneKeyA, kFakePhoneNameA);

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegateRemote();
  // Check the delegate initializes to empty.
  EXPECT_EQ(GetMostRecentVerifiedHostDeviceIdPref(), "");

  // Set initially verified host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhone);
  EXPECT_EQ(GetMostRecentVerifiedHostDeviceIdPref(), fakePhone.GetDeviceId());

  // Switch to an unverified host.
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified, fakePhoneA);
  // The host is set but not verified so the pref should be set to empty.
  EXPECT_EQ(GetMostRecentVerifiedHostDeviceIdPref(), "");

  // Verify the new host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, fakePhoneA);
  EXPECT_EQ(GetMostRecentVerifiedHostDeviceIdPref(), fakePhoneA.GetDeviceId());

  ForgetHost();
  EXPECT_EQ(GetMostRecentVerifiedHostDeviceIdPref(), "");
}

}  // namespace multidevice_setup

}  // namespace ash
