// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager_impl.h"

#include <memory>

#include "ash/webui/eche_app_ui/pref_names.h"
#include "chromeos/ash/components/phonehub/combined_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/fake_connection_scheduler.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"
#include "chromeos/ash/components/phonehub/notification_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

using AccessStatus = MultideviceFeatureAccessManager::AccessStatus;
using AccessProhibitedReason =
    MultideviceFeatureAccessManager::AccessProhibitedReason;
using FeatureState = multidevice_setup::mojom::FeatureState;
using Feature = multidevice_setup::mojom::Feature;

class FakeObserver : public MultideviceFeatureAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override { ++num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnCameraRollAccessChanged() override { ++num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnFeatureSetupRequestSupportedChanged() override { ++num_calls_; }

  // MultideviceFeatureAccessManager::Observer:
  void OnAppsAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeNotificationAccessSetupOperationDelegate
    : public NotificationAccessSetupOperation::Delegate {
 public:
  FakeNotificationAccessSetupOperationDelegate() = default;
  ~FakeNotificationAccessSetupOperationDelegate() override = default;

  NotificationAccessSetupOperation::Status status() const { return status_; }

  // NotificationAccessSetupOperation::Delegate:
  void OnNotificationStatusChange(
      NotificationAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  NotificationAccessSetupOperation::Status status_ =
      NotificationAccessSetupOperation::Status::kConnecting;
};

class FakeCombinedAccessSetupOperationDelegate
    : public CombinedAccessSetupOperation::Delegate {
 public:
  FakeCombinedAccessSetupOperationDelegate() = default;
  ~FakeCombinedAccessSetupOperationDelegate() override = default;

  CombinedAccessSetupOperation::Status status() const { return status_; }

  // CombinedAccessSetupOperation::Delegate:
  void OnCombinedStatusChange(
      CombinedAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  CombinedAccessSetupOperation::Status status_ =
      CombinedAccessSetupOperation::Status::kConnecting;
};

class FakeFeatureSetupConnectionOperationDelegate
    : public FeatureSetupConnectionOperation::Delegate {
 public:
  FakeFeatureSetupConnectionOperationDelegate() = default;
  ~FakeFeatureSetupConnectionOperationDelegate() override = default;

  FeatureSetupConnectionOperation::Status status() const { return status_; }

  void OnFeatureSetupConnectionStatusChange(
      FeatureSetupConnectionOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  FeatureSetupConnectionOperation::Status status_ =
      FeatureSetupConnectionOperation::Status::kConnecting;
};

}  // namespace

class MultideviceFeatureAccessManagerImplTest : public testing::Test {
 protected:
  MultideviceFeatureAccessManagerImplTest() = default;
  MultideviceFeatureAccessManagerImplTest(
      const MultideviceFeatureAccessManagerImplTest&) = delete;
  MultideviceFeatureAccessManagerImplTest& operator=(
      const MultideviceFeatureAccessManagerImplTest&) = delete;
  ~MultideviceFeatureAccessManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    MultideviceFeatureAccessManagerImpl::RegisterPrefs(
        pref_service_.registry());
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    fake_connection_scheduler_ = std::make_unique<FakeConnectionScheduler>();
    pref_service_.registry()->RegisterIntegerPref(
        eche_app::prefs::kAppsAccessStatus,
        /*default_value=*/0);
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  void InitializeAccessStatus(
      AccessStatus notification_expected_status,
      AccessStatus camera_roll_expected_status,
      AccessProhibitedReason reason = AccessProhibitedReason::kUnknown,
      bool feature_setup_request_supported = false) {
    pref_service_.SetInteger(prefs::kNotificationAccessStatus,
                             static_cast<int>(notification_expected_status));
    pref_service_.SetInteger(prefs::kCameraRollAccessStatus,
                             static_cast<int>(camera_roll_expected_status));
    pref_service_.SetInteger(prefs::kNotificationAccessProhibitedReason,
                             static_cast<int>(reason));
    pref_service_.SetBoolean(prefs::kFeatureSetupRequestSupported,
                             feature_setup_request_supported);
    SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/false);
    manager_ = std::make_unique<MultideviceFeatureAccessManagerImpl>(
        &pref_service_, fake_multidevice_setup_client_.get(),
        fake_feature_status_provider_.get(), fake_message_sender_.get(),
        fake_connection_scheduler_.get());
    manager_->AddObserver(&fake_observer_);
  }

  void InitializeAppsAccessStatus(MultideviceFeatureAccessManager::AccessStatus
                                      apps_access_expected_status) {
    pref_service_.SetInteger(eche_app::prefs::kAppsAccessStatus,
                             static_cast<int>(apps_access_expected_status));
  }

  NotificationAccessSetupOperation::Status
  GetNotificationAccessSetupOperationStatus() {
    return fake_notification_delegate_.status();
  }

  CombinedAccessSetupOperation::Status GetCombinedAccessSetupOperationStatus() {
    return fake_combined_delegate_.status();
  }

  FeatureSetupConnectionOperation::Status
  GetFeatureSetupConnectionOperationStatus() {
    return fake_connection_delegate_.status();
  }

  void VerifyNotificationAccessGrantedState(AccessStatus expected_status) {
    VerifyNotificationAccessGrantedState(expected_status,
                                         AccessProhibitedReason::kUnknown);
  }

  void VerifyNotificationAccessGrantedState(
      AccessStatus expected_status,
      AccessProhibitedReason expected_reason) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kNotificationAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetNotificationAccessStatus());
    EXPECT_EQ(
        static_cast<int>(expected_reason),
        pref_service_.GetInteger(prefs::kNotificationAccessProhibitedReason));
    EXPECT_EQ(expected_reason,
              manager_->GetNotificationAccessProhibitedReason());
  }

  void VerifyCameraRollAccessGrantedState(AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kCameraRollAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetCameraRollAccessStatus());
  }

  void VerifyAppsAccessGrantedState(
      MultideviceFeatureAccessManager::AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(eche_app::prefs::kAppsAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetAppsAccessStatus());
  }

  void VerifyFeatureSetupRequestSupported(bool expected) {
    EXPECT_EQ(expected,
              pref_service_.GetBoolean(prefs::kFeatureSetupRequestSupported));
    EXPECT_EQ(expected, manager_->GetFeatureSetupRequestSupported());
  }

  bool HasMultideviceFeatureSetupUiBeenDismissed() {
    return manager_->HasMultideviceFeatureSetupUiBeenDismissed();
  }

  void DismissSetupRequiredUi() { manager_->DismissSetupRequiredUi(); }

  std::unique_ptr<NotificationAccessSetupOperation>
  StartNotificationSetupOperation() {
    return manager_->AttemptNotificationSetup(&fake_notification_delegate_);
  }
  std::unique_ptr<CombinedAccessSetupOperation> StartCombinedSetupOperation(
      bool camera_roll,
      bool notifications) {
    return manager_->AttemptCombinedFeatureSetup(camera_roll, notifications,
                                                 &fake_combined_delegate_);
  }
  std::unique_ptr<FeatureSetupConnectionOperation>
  StartFeatureSetupConnectionOperation() {
    return manager_->AttemptFeatureSetupConnection(&fake_connection_delegate_);
  }

  bool IsNotificationSetupOperationInProgress() {
    return manager_->IsNotificationSetupOperationInProgress();
  }

  void SetNotificationAccessStatusInternal(AccessStatus status,
                                           AccessProhibitedReason reason) {
    manager_->SetNotificationAccessStatusInternal(status, reason);
  }

  void SetCameraRollAccessStatusInternal(AccessStatus status) {
    manager_->SetCameraRollAccessStatusInternal(status);
  }

  void SetFeatureSetupRequestSupportedInternal(bool supported) {
    manager_->SetFeatureSetupRequestSupportedInternal(supported);
  }

  void SetFeatureStatus(FeatureStatus status) {
    PA_LOG(INFO) << "status changed to " << status;
    fake_feature_status_provider_->SetStatus(status);
  }

  FeatureStatus GetFeatureStatus() {
    return fake_feature_status_provider_->GetStatus();
  }

  size_t GetNumScheduleConnectionNowCalls() const {
    return fake_connection_scheduler_->num_schedule_connection_now_calls();
  }

  size_t GetNumShowNotificationAccessSetupRequestCount() const {
    return fake_message_sender_->show_notification_access_setup_request_count();
  }

  size_t GetCombinedAccessSetupRequestCallCount() const {
    return fake_message_sender_->GetFeatureSetupRequestCallCount();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  void SetNeedsOneTimeNotificationAccessUpdate(bool needs_update) {
    pref_service_.SetBoolean(prefs::kNeedsOneTimeNotificationAccessUpdate,
                             needs_update);
  }

  void SetEcheFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kEche, feature_state);
  }

  bool IsAccessRequestAllowed(Feature feature) {
    return manager_->IsAccessRequestAllowed(feature);
  }

  void UpdatedFeatureSetupConnectionStatusIfNeeded() {
    manager_->UpdatedFeatureSetupConnectionStatusIfNeeded();
  }

 private:
  TestingPrefServiceSimple pref_service_;

  FakeObserver fake_observer_;
  FakeNotificationAccessSetupOperationDelegate fake_notification_delegate_;
  FakeCombinedAccessSetupOperationDelegate fake_combined_delegate_;
  FakeFeatureSetupConnectionOperationDelegate fake_connection_delegate_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<FakeConnectionScheduler> fake_connection_scheduler_;
  std::unique_ptr<MultideviceFeatureAccessManager> manager_;
};

TEST_F(MultideviceFeatureAccessManagerImplTest, FeatureNotReadyForAccess) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  SetEcheFeatureState(FeatureState::kNotSupportedByChromebook);

  EXPECT_FALSE(IsAccessRequestAllowed(Feature::kEche));
}

TEST_F(MultideviceFeatureAccessManagerImplTest, FeatureReadyForAccess) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  SetEcheFeatureState(FeatureState::kEnabledByUser);

  EXPECT_TRUE(IsAccessRequestAllowed(Feature::kEche));

  SetEcheFeatureState(FeatureState::kDisabledByUser);

  EXPECT_TRUE(IsAccessRequestAllowed(Feature::kEche));
}

TEST_F(MultideviceFeatureAccessManagerImplTest, ShouldShowSetupRequiredUi) {
  // Notification setup is not dismissed initially even when access has been
  // granted.
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_FALSE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Notification setup is not dismissed initially when access has not been
  // granted.
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_FALSE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Simlulate dismissal of UI.
  DismissSetupRequiredUi();
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access not granted.
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access granted.
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  EXPECT_TRUE(HasMultideviceFeatureSetupUiBeenDismissed());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartSetupConnectionFromDisconnected) {
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);

  // Initial operation status should be connecting
  auto operation = StartFeatureSetupConnectionOperation();
  EXPECT_TRUE(operation);

  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kTimedOutConnecting,
            GetFeatureSetupConnectionOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartSetupConnectionFromBluetoothDisabled) {
  SetFeatureStatus(FeatureStatus::kUnavailableBluetoothOff);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);

  // Initial operation status should be connecting.
  auto operation = StartFeatureSetupConnectionOperation();
  EXPECT_TRUE(operation);

  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  UpdatedFeatureSetupConnectionStatusIfNeeded();
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kConnected,
            GetFeatureSetupConnectionOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartSetupConnectionResultConnected) {
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);

  // Initial operation status should be connecting
  auto operation = StartFeatureSetupConnectionOperation();
  EXPECT_TRUE(operation);

  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  // Should remain connecting until PhoneStatusSnapshot is received
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  UpdatedFeatureSetupConnectionStatusIfNeeded();
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kConnected,
            GetFeatureSetupConnectionOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartSetupConnectionFromConnected) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);

  // Start connecting
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kConnecting,
            GetFeatureSetupConnectionOperationStatus());

  // Initial operation status should be connecting
  auto operation = StartFeatureSetupConnectionOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kConnected,
            GetFeatureSetupConnectionOperationStatus());

  // Should remain connecting until PhoneStatusSnapshot is received
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(FeatureSetupConnectionOperation::Status::kConnectionLost,
            GetFeatureSetupConnectionOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, AllAccessInitiallyGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Cannot start the notification access setup flow if notification and camera
  // roll access have already been granted.
  auto operation = StartNotificationSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest, OnFeatureStatusChanged) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotificationAccessSetupOperationStatus());
  // Simulate feature status to be enabled and connected. SetupOperation is
  // also not in progress, so expect no new requests to be sent.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotificationAccessSetupOperationStatus());
  // Simulate setup operation is in progress. This will trigger a sent
  // request.
  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Set another feature status, expect status to be updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartDisconnectedAndNoAccess) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       StartDisconnectedAndNoAccess_NotificationAccessIsProhibited) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(
      NotificationAccessSetupOperation::Status::kProhibitedFromProvidingAccess,
      GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartConnectingAndNoAccess) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, StartConnectedAndNoAccess) {
  // Set initial state to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotificationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       SimulateConnectingToDisconnected) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kTimedOutConnecting,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       SimulateConnectedToDisconnected) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate a disconnection, expect status update.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, SimulateConnectedToDisabled) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);

  auto operation = StartNotificationSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotificationAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipNotificationAccessGrantedToNotGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping notification access state to no granted.
  SetNotificationAccessStatusInternal(AccessStatus::kAvailableButNotGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipNotificationAccessGrantedToProhibited) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping notification access state to prohibited.
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipCameraRollAccessGrantedToNotGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Simulate flipping camera roll access state to no granted.
  SetCameraRollAccessStatusInternal(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, AccessNotChanged) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // If the access state is unchanged, we do not expect any notifications.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NeedsOneTimeNotificationAccessUpdate_AccessGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Send a one-time signal to observers if access is granted. See
  // http://crbug.com/1215559.
  SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/true);
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Observers should be notified only once ever.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NeedsOneTimeNotificationAccessUpdate_Prohibited) {
  InitializeAccessStatus(AccessStatus::kProhibited,
                         AccessStatus::kAccessGranted,
                         AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);

  // Only send the one-time signal to observers if access is granted. See
  // http://crbug.com/1215559.
  SetNeedsOneTimeNotificationAccessUpdate(/*needs_update=*/true);
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(0u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NotificationAccessProhibitedReason_FromProhibited) {
  InitializeAccessStatus(AccessStatus::kProhibited,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited);

  // Simulates an initial update after the pref is first added
  SetNotificationAccessStatusInternal(AccessStatus::kProhibited,
                                      AccessProhibitedReason::kWorkProfile);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited,
                                       AccessProhibitedReason::kWorkProfile);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // No update or observer notification should occur with no change
  SetNotificationAccessStatusInternal(AccessStatus::kProhibited,
                                      AccessProhibitedReason::kWorkProfile);
  VerifyNotificationAccessGrantedState(AccessStatus::kProhibited,
                                       AccessProhibitedReason::kWorkProfile);
  EXPECT_EQ(1u, GetNumObserverCalls());

  // This can happen if a user updates from Android <N to >=N
  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       NotificationAccessProhibitedReason_FromGranted) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);

  SetNotificationAccessStatusInternal(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  VerifyNotificationAccessGrantedState(
      AccessStatus::kProhibited,
      AccessProhibitedReason::kDisabledByPhonePolicy);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest, AppsAccessChanged) {
  InitializeAccessStatus(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted,
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted);
  InitializeAppsAccessStatus(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted);
  VerifyAppsAccessGrantedState(
      MultideviceFeatureAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(1u, GetNumObserverCalls());

  InitializeAppsAccessStatus(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyAppsAccessGrantedState(
      MultideviceFeatureAccessManager::AccessStatus::kAvailableButNotGranted);
  EXPECT_EQ(2u, GetNumObserverCalls());

  InitializeAppsAccessStatus(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited);
  VerifyAppsAccessGrantedState(
      MultideviceFeatureAccessManager::AccessStatus::kProhibited);
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       FlipFeatureSetupRequestSupportedOn) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(false);

  SetFeatureSetupRequestSupportedInternal(true);
  VerifyFeatureSetupRequestSupported(true);
  EXPECT_EQ(1u, GetNumObserverCalls());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_FeatureSetupRequestNotSupported) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(false);

  // Cannot start the combined access setup flow if FeatureSetupRequest is not
  // supported.
  auto operation =
      StartCombinedSetupOperation(/*camera_roll=*/true, /*notifications=*/true);
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_AllFeaturesGranted_AllFeaturesRequested) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAccessGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Cannot start the combined access setup flow if requested feature access
  // has already been granted.
  auto operation =
      StartCombinedSetupOperation(/*camera_roll=*/true, /*notifications=*/true);
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_CameraRollGranted_AllFeaturesRequested) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Cannot start the combined access setup flow if requested feature access
  // has already been granted.
  auto operation =
      StartCombinedSetupOperation(/*camera_roll=*/true, /*notifications=*/true);
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_NotificationsGranted_AllFeaturesRequested) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAvailableButNotGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Cannot start the combined access setup flow if requested feature access
  // has already been granted.
  auto operation =
      StartCombinedSetupOperation(/*camera_roll=*/true, /*notifications=*/true);
  EXPECT_FALSE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_CameraRollGranted_NotificationsRequested) {
  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAccessGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Can start the combined access setup flow if requested feature access is not
  // granted, even if other feature is granted.
  auto operation = StartCombinedSetupOperation(/*camera_roll=*/false,
                                               /*notifications=*/true);
  EXPECT_TRUE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_NotificationsGranted_CameraRollRequested) {
  InitializeAccessStatus(AccessStatus::kAccessGranted,
                         AccessStatus::kAvailableButNotGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Can start the combined access setup flow if requested feature access is not
  // granted, even if other feature is granted.
  auto operation = StartCombinedSetupOperation(/*camera_roll=*/true,
                                               /*notifications=*/false);
  EXPECT_TRUE(operation);
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_FullSetupFromDisconnected) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Start combined setup operation
  auto operation = StartCombinedSetupOperation(/*camera_roll=*/true,
                                               /*notifications=*/true);
  EXPECT_TRUE(operation);
  EXPECT_EQ(0u, GetCombinedAccessSetupRequestCallCount());
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kConnecting,
            GetCombinedAccessSetupOperationStatus());
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(0u, GetCombinedAccessSetupRequestCallCount());
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kConnecting,
            GetCombinedAccessSetupOperationStatus());
  EXPECT_EQ(1u, GetNumScheduleConnectionNowCalls());

  // Simulate changing state to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(1u, GetCombinedAccessSetupRequestCallCount());
  EXPECT_EQ(CombinedAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetCombinedAccessSetupOperationStatus());

  // Simulate Camera Roll being granted on phone.
  SetCameraRollAccessStatusInternal(AccessStatus::kAccessGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(CombinedAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetCombinedAccessSetupOperationStatus());

  // Simulate Notifications being granted on phone.
  SetNotificationAccessStatusInternal(AccessStatus::kAccessGranted,
                                      AccessProhibitedReason::kUnknown);
  VerifyNotificationAccessGrantedState(AccessStatus::kAccessGranted);
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kCompletedSuccessfully,
            GetCombinedAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_SimulateTimeout) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Start combined setup operation
  auto operation = StartCombinedSetupOperation(/*camera_roll=*/true,
                                               /*notifications=*/true);
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kTimedOutConnecting,
            GetCombinedAccessSetupOperationStatus());
}

TEST_F(MultideviceFeatureAccessManagerImplTest,
       CombinedFeatureSetup_SimulateDisconnect) {
  // Set initial state to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  InitializeAccessStatus(AccessStatus::kAvailableButNotGranted,
                         AccessStatus::kAvailableButNotGranted,
                         AccessProhibitedReason::kUnknown,
                         /*feature_setup_request_supported=*/true);
  VerifyNotificationAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyCameraRollAccessGrantedState(AccessStatus::kAvailableButNotGranted);
  VerifyFeatureSetupRequestSupported(true);

  // Start combined setup operation
  auto operation = StartCombinedSetupOperation(/*camera_roll=*/true,
                                               /*notifications=*/true);
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(CombinedAccessSetupOperation::Status::kConnectionDisconnected,
            GetCombinedAccessSetupOperationStatus());
}

}  // namespace phonehub
}  // namespace ash
