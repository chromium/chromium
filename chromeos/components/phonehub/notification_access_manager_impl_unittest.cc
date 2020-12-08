// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_manager_impl.h"

#include <memory>

#include "chromeos/components/phonehub/fake_connection_scheduler.h"
#include "chromeos/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_message_sender.h"
#include "chromeos/components/phonehub/notification_access_setup_operation.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public NotificationAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // NotificationAccessManager::Observer:
  void OnNotificationAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeOperationDelegate
    : public NotificationAccessSetupOperation::Delegate {
 public:
  FakeOperationDelegate() = default;
  ~FakeOperationDelegate() override = default;

  NotificationAccessSetupOperation::Status status() const { return status_; }

  // NotificationAccessSetupOperation::Delegate:
  void OnStatusChange(
      NotificationAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  NotificationAccessSetupOperation::Status status_ =
      NotificationAccessSetupOperation::Status::kConnecting;
};

}  // namespace

class NotificationAccessManagerImplTest : public testing::Test {
 protected:
  NotificationAccessManagerImplTest() = default;
  NotificationAccessManagerImplTest(const NotificationAccessManagerImplTest&) =
      delete;
  NotificationAccessManagerImplTest& operator=(
      const NotificationAccessManagerImplTest&) = delete;
  ~NotificationAccessManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    NotificationAccessManagerImpl::RegisterPrefs(pref_service_.registry());
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    fake_connection_scheduler_ = std::make_unique<FakeConnectionScheduler>();
  }

  void TearDown() override { manager_->RemoveObserver(&fake_observer_); }

  void Initialize(NotificationAccessManager::AccessStatus expected_status) {
    pref_service_.SetInteger(prefs::kNotificationAccessStatus,
                             static_cast<int>(expected_status));
    manager_ = std::make_unique<NotificationAccessManagerImpl>(
        &pref_service_, fake_feature_status_provider_.get(),
        fake_message_sender_.get(), fake_connection_scheduler_.get());
    manager_->AddObserver(&fake_observer_);
  }

  NotificationAccessSetupOperation::Status
  GetNotifcationAccessSetupOperationStatus() {
    return fake_delegate_.status();
  }

  void VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus expected_status) {
    EXPECT_EQ(static_cast<int>(expected_status),
              pref_service_.GetInteger(prefs::kNotificationAccessStatus));
    EXPECT_EQ(expected_status, manager_->GetAccessStatus());
  }

  bool HasNotificationSetupUiBeenDismissed() {
    return manager_->HasNotificationSetupUiBeenDismissed();
  }

  void DismissSetupRequiredUi() { manager_->DismissSetupRequiredUi(); }

  std::unique_ptr<NotificationAccessSetupOperation> StartSetupOperation() {
    return manager_->AttemptNotificationSetup(&fake_delegate_);
  }

  bool IsSetupOperationInProgress() {
    return manager_->IsSetupOperationInProgress();
  }

  void SetAccessStatusInternal(NotificationAccessManager::AccessStatus status) {
    manager_->SetAccessStatusInternal(status);
  }

  void SetFeatureStatus(FeatureStatus status) {
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

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  TestingPrefServiceSimple pref_service_;

  FakeObserver fake_observer_;
  FakeOperationDelegate fake_delegate_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<FakeConnectionScheduler> fake_connection_scheduler_;
  std::unique_ptr<NotificationAccessManager> manager_;
};

TEST_F(NotificationAccessManagerImplTest, ShouldShowSetupRequiredUi) {
  // Notification setup is not dismissed initially even when access has been
  // granted.
  Initialize(NotificationAccessManager::AccessStatus::kAccessGranted);
  EXPECT_FALSE(HasNotificationSetupUiBeenDismissed());

  // Notification setup is not dismissed initially when access has not been
  // granted.
  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  EXPECT_FALSE(HasNotificationSetupUiBeenDismissed());

  // Simlulate dismissal of UI.
  DismissSetupRequiredUi();
  EXPECT_TRUE(HasNotificationSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access not granted.
  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  EXPECT_TRUE(HasNotificationSetupUiBeenDismissed());

  // Dismissal value is persisted on initialization with access granted.
  Initialize(NotificationAccessManager::AccessStatus::kAccessGranted);
  EXPECT_TRUE(HasNotificationSetupUiBeenDismissed());
}

TEST_F(NotificationAccessManagerImplTest, InitiallyGranted) {
  Initialize(NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);

  // Cannot start the notification access setup flow if access has already been
  // granted.
  auto operation = StartSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(NotificationAccessManagerImplTest, OnFeatureStatusChanged) {
  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotifcationAccessSetupOperationStatus());

  // Simulate feature status to be enabled and connected. SetupOperation is also
  // not in progress, so expect no new requests to be sent.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(0u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnecting,
            GetNotifcationAccessSetupOperationStatus());

  // Simulate setup operation is in progress. This will trigger a sent request.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotifcationAccessSetupOperationStatus());

  // Set another feature status, expect status to be updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, StartDisconnectedAndNoAccess) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
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
            GetNotifcationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetAccessStatusInternal(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest,
       StartDisconnectedAndNoAccess_Prohibited) {
  // Set initial state to disconnected.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled but disconnected status and access
  // not granted.
  auto operation = StartSetupOperation();
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
            GetNotifcationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetAccessStatusInternal(NotificationAccessManager::AccessStatus::kProhibited);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kProhibited);
  EXPECT_EQ(
      NotificationAccessSetupOperation::Status::kProhibitedFromProvidingAccess,
      GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, StartConnectingAndNoAccess) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connecting status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate changing states from connecting to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotifcationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetAccessStatusInternal(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, StartConnectedAndNoAccess) {
  // Set initial state to connected.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  // Start a setup operation with enabled and connected status and access
  // not granted.
  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Verify that the request message has been sent and our operation status
  // is updated.
  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());
  EXPECT_EQ(NotificationAccessSetupOperation::Status::
                kSentMessageToPhoneAndWaitingForResponse,
            GetNotifcationAccessSetupOperationStatus());

  // Simulate getting a response back from the phone.
  SetAccessStatusInternal(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, SimulateConnectingToDisconnected) {
  // Set initial state to connecting.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnecting);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  // Simulate a disconnection and expect that status has been updated.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kTimedOutConnecting,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, SimulateConnectedToDisconnected) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate a disconnection, expect status update.
  SetFeatureStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, SimulateConnectedToDisabled) {
  // Simulate connected state.
  SetFeatureStatus(FeatureStatus::kEnabledAndConnected);

  Initialize(NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);

  EXPECT_EQ(1u, GetNumShowNotificationAccessSetupRequestCount());

  // Simulate disabling the feature, expect status update.
  SetFeatureStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(NotificationAccessSetupOperation::Status::kConnectionDisconnected,
            GetNotifcationAccessSetupOperationStatus());
}

TEST_F(NotificationAccessManagerImplTest, FlipAccessGrantedToNotGranted) {
  Initialize(NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);

  // Simulate flipping the access state to no granted.
  SetAccessStatusInternal(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted);
}

TEST_F(NotificationAccessManagerImplTest, FlipAccessGrantedToProhibited) {
  Initialize(NotificationAccessManager::AccessStatus::kAccessGranted);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kAccessGranted);

  // Simulate flipping the access state to prohibited.
  SetAccessStatusInternal(NotificationAccessManager::AccessStatus::kProhibited);
  VerifyNotificationAccessGrantedState(
      NotificationAccessManager::AccessStatus::kProhibited);
}

}  // namespace phonehub
}  // namespace chromeos
