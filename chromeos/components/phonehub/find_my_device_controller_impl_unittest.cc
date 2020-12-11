// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/find_my_device_controller_impl.h"

#include <memory>

#include "chromeos/components/phonehub/fake_do_not_disturb_controller.h"
#include "chromeos/components/phonehub/fake_message_sender.h"
#include "chromeos/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/components/phonehub/find_my_device_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public FindMyDeviceController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // FindMyDeviceController::Observer:
  void OnPhoneRingingStateChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class FindMyDeviceControllerImplTest : public testing::Test {
 protected:
  FindMyDeviceControllerImplTest() = default;
  FindMyDeviceControllerImplTest(const FindMyDeviceControllerImplTest&) =
      delete;
  FindMyDeviceControllerImplTest& operator=(
      const FindMyDeviceControllerImplTest&) = delete;
  ~FindMyDeviceControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ = std::make_unique<FindMyDeviceControllerImpl>(
        &fake_do_not_disturb_controller_, &fake_message_sender_,
        &fake_user_action_recorder_);
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  FindMyDeviceController::Status GetPhoneRingingStatus() const {
    return controller_->GetPhoneRingingStatus();
  }

  void SetIsPhoneRingingInternal(bool is_phone_ringing) {
    controller_->SetIsPhoneRingingInternal(is_phone_ringing);
  }

  void RequestNewPhoneRingingState(bool ringing) {
    controller_->RequestNewPhoneRingingState(ringing);
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 protected:
  FakeDoNotDisturbController fake_do_not_disturb_controller_;
  FakeMessageSender fake_message_sender_;
  FakeUserActionRecorder fake_user_action_recorder_;

 private:
  std::unique_ptr<FindMyDeviceControllerImpl> controller_;
  FakeObserver fake_observer_;
};

TEST_F(FindMyDeviceControllerImplTest, RingingStateChanges) {
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOff,
            GetPhoneRingingStatus());

  // Simulate flipping DoNotDisturb mode to enabled, this should set the
  // FindMyPhone status to kRingingNotAvailable.
  fake_do_not_disturb_controller_.SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/true, /*can_request_new_dnd_state=*/true);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            GetPhoneRingingStatus());
  // Simulate initiating phone ringing when DoNotDisturb mode is enabled. This
  // will not update the internal status.
  SetIsPhoneRingingInternal(/*is_phone_ringing=*/true);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            GetPhoneRingingStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Flip DoNotDisturb back to disabled, expect status to reset back to its
  // previous state.
  fake_do_not_disturb_controller_.SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/false, /*can_request_new_dnd_state=*/true);
  // Since we previously recorded that the phone should be ringing during
  // DoNotDisturb mode was enabled, we return to that state once DoNotDisturb
  // is disabled.
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            GetPhoneRingingStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Attempt to set ringing status with the same previous state. Expect that no
  // observer calls were made.
  SetIsPhoneRingingInternal(/*is_phone_ringing=*/true);
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(FindMyDeviceControllerImplTest, RequestNewRingStatus) {
  RequestNewPhoneRingingState(/*ringing=*/true);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(1u, fake_message_sender_.GetRingDeviceRequestCallCount());
  EXPECT_TRUE(fake_message_sender_.GetRecentRingDeviceRequest());

  // Simulate flipping DoNotDisturb mode to enabled, this should set the
  // FindMyPhone status to kRingingNotAvailable and not send any new messages.
  fake_do_not_disturb_controller_.SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/true, /*can_request_new_dnd_state=*/true);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            GetPhoneRingingStatus());

  RequestNewPhoneRingingState(/*ringing=*/false);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(1u, fake_message_sender_.GetRingDeviceRequestCallCount());
  // No new messages were sent, expect that the last request was still the
  // previous "true" value.
  EXPECT_TRUE(fake_message_sender_.GetRecentRingDeviceRequest());

  // Flip DoNotDisturb mode to disabled, expect that messages are able to be
  // sent again.
  fake_do_not_disturb_controller_.SetDoNotDisturbStateInternal(
      /*is_dnd_enabled=*/false, /*can_request_new_dnd_state=*/true);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOff,
            GetPhoneRingingStatus());

  RequestNewPhoneRingingState(/*ringing=*/false);
  EXPECT_EQ(2u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(2u, fake_message_sender_.GetRingDeviceRequestCallCount());
  EXPECT_FALSE(fake_message_sender_.GetRecentRingDeviceRequest());
}

}  // namespace phonehub
}  // namespace chromeos
