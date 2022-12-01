// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/do_not_disturb_controller_impl.h"

#include <memory>

#include "chromeos/ash/components/phonehub/fake_message_sender.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {
namespace {

class FakeObserver : public DoNotDisturbController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // DoNotDisturbController::Observer:
  void OnDndStateChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class DoNotDisturbControllerImplTest : public testing::Test {
 protected:
  DoNotDisturbControllerImplTest() = default;
  DoNotDisturbControllerImplTest(const DoNotDisturbControllerImplTest&) =
      delete;
  DoNotDisturbControllerImplTest& operator=(
      const DoNotDisturbControllerImplTest&) = delete;
  ~DoNotDisturbControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ = std::make_unique<DoNotDisturbControllerImpl>(
        &fake_message_sender_, &fake_user_action_recorder_);
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  bool IsDndEnabled() const { return controller_->IsDndEnabled(); }

  bool CanRequestNewDndState() const {
    return controller_->CanRequestNewDndState();
  }

  void SetDoNotDisturbInternal(bool is_dnd_enabled,
                               bool can_request_new_dnd_state) {
    controller_->SetDoNotDisturbStateInternal(is_dnd_enabled,
                                              can_request_new_dnd_state);
  }

  void RequestNewDoNotDisturbState(bool enabled) {
    controller_->RequestNewDoNotDisturbState(enabled);
  }

  bool GetRecentUpdateNotificationModeRequest() {
    return fake_message_sender_.GetRecentUpdateNotificationModeRequest();
  }

  size_t GetUpdateNotificationModeRequestCallCount() {
    return fake_message_sender_.GetUpdateNotificationModeRequestCallCount();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

  FakeUserActionRecorder fake_user_action_recorder_;

 private:
  FakeObserver fake_observer_;

  FakeMessageSender fake_message_sender_;

  std::unique_ptr<DoNotDisturbControllerImpl> controller_;
};

TEST_F(DoNotDisturbControllerImplTest, SetInternalStatesWithObservers) {
  EXPECT_FALSE(IsDndEnabled());

  SetDoNotDisturbInternal(/*is_dnd_enabled=*/true,
                          /*can_request_new_dnd_state=*/true);
  EXPECT_TRUE(IsDndEnabled());
  EXPECT_TRUE(CanRequestNewDndState());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false,
                          /*can_request_new_dnd_state=*/true);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_TRUE(CanRequestNewDndState());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Setting internal dnd state with the same previous state will not trigger an
  // observer event.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false,
                          /*can_request_new_dnd_state=*/true);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_TRUE(CanRequestNewDndState());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Changing |can_request_new_dnd_state| should also trigger an observer event.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false,
                          /*can_request_new_dnd_state=*/false);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_FALSE(CanRequestNewDndState());
  EXPECT_EQ(3u, GetNumObserverCalls());

  // No new changes, expect no observer call.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false,
                          /*can_request_new_dnd_state=*/false);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_FALSE(CanRequestNewDndState());
  EXPECT_EQ(3u, GetNumObserverCalls());

  // Both states are changed, expect an observer call.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/true,
                          /*can_request_new_dnd_state=*/true);
  EXPECT_TRUE(IsDndEnabled());
  EXPECT_TRUE(CanRequestNewDndState());
  EXPECT_EQ(4u, GetNumObserverCalls());
}

TEST_F(DoNotDisturbControllerImplTest, RequestNewDoNotDisturbState) {
  RequestNewDoNotDisturbState(/*enabled=*/true);
  EXPECT_EQ(1u, fake_user_action_recorder_.num_dnd_attempts());
  EXPECT_TRUE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(1u, GetUpdateNotificationModeRequestCallCount());
  // Simulate receiving a response and setting the internal value.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/true,
                          /*can_request_new_dnd_state=*/true);

  RequestNewDoNotDisturbState(/*enabled=*/false);
  EXPECT_EQ(2u, fake_user_action_recorder_.num_dnd_attempts());
  EXPECT_FALSE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(2u, GetUpdateNotificationModeRequestCallCount());
  // Simulate receiving a response and setting the internal value.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false,
                          /*can_request_new_dnd_state=*/true);

  // Requesting for a the same state as the currently set state is a no-op.
  RequestNewDoNotDisturbState(/*enabled=*/false);
  EXPECT_EQ(2u, fake_user_action_recorder_.num_dnd_attempts());
  EXPECT_FALSE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(2u, GetUpdateNotificationModeRequestCallCount());
}

}  // namespace phonehub
}  // namespace ash
