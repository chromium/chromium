// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/find_my_device_controller_impl.h"

#include <memory>

#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/ash/components/phonehub/find_my_device_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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
        &fake_message_sender_, &fake_user_action_recorder_);
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  FindMyDeviceController::Status GetPhoneRingingStatus() const {
    return controller_->GetPhoneRingingStatus();
  }

  void SetPhoneRingingStatusInternal(FindMyDeviceController::Status status) {
    controller_->SetPhoneRingingStatusInternal(status);
  }

  void RequestNewPhoneRingingState(bool ringing) {
    controller_->RequestNewPhoneRingingState(ringing);
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 protected:
  FakeMessageSender fake_message_sender_;
  FakeUserActionRecorder fake_user_action_recorder_;

 private:
  std::unique_ptr<FindMyDeviceControllerImpl> controller_;
  FakeObserver fake_observer_;
};

TEST_F(FindMyDeviceControllerImplTest, RingingStateChanges) {
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOff,
            GetPhoneRingingStatus());

  SetPhoneRingingStatusInternal(FindMyDeviceController::Status::kRingingOn);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            GetPhoneRingingStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetPhoneRingingStatusInternal(
      FindMyDeviceController::Status::kRingingNotAvailable);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            GetPhoneRingingStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetPhoneRingingStatusInternal(FindMyDeviceController::Status::kRingingOff);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOff,
            GetPhoneRingingStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());

  // Set the current value; observers should not be notified.
  SetPhoneRingingStatusInternal(FindMyDeviceController::Status::kRingingOff);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOff,
            GetPhoneRingingStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FindMyDeviceControllerImplTest, RequestNewRingStatus) {
  RequestNewPhoneRingingState(/*ringing=*/true);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(1u, fake_message_sender_.GetRingDeviceRequestCallCount());
  EXPECT_TRUE(fake_message_sender_.GetRecentRingDeviceRequest());

  // Change status to "not available".
  SetPhoneRingingStatusInternal(
      FindMyDeviceController::Status::kRingingNotAvailable);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingNotAvailable,
            GetPhoneRingingStatus());

  // Requesting new state should fail since it is unavailable.
  RequestNewPhoneRingingState(/*ringing=*/true);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(1u, fake_message_sender_.GetRingDeviceRequestCallCount());

  // Change out of "not available".
  SetPhoneRingingStatusInternal(FindMyDeviceController::Status::kRingingOn);
  EXPECT_EQ(FindMyDeviceController::Status::kRingingOn,
            GetPhoneRingingStatus());

  RequestNewPhoneRingingState(/*ringing=*/false);
  EXPECT_EQ(2u, fake_user_action_recorder_.num_find_my_device_attempts());
  EXPECT_EQ(2u, fake_message_sender_.GetRingDeviceRequestCallCount());
  EXPECT_FALSE(fake_message_sender_.GetRecentRingDeviceRequest());
}

}  // namespace phonehub
}  // namespace ash
