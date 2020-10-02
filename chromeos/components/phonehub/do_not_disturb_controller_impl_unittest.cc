// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/do_not_disturb_controller_impl.h"

#include <memory>

#include "chromeos/components/phonehub/fake_message_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
    fake_message_sender_ = std::make_unique<FakeMessageSender>();
    controller_ = std::make_unique<DoNotDisturbControllerImpl>(
        fake_message_sender_.get());
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  bool IsDndEnabled() const { return controller_->IsDndEnabled(); }

  void SetDoNotDisturbInternal(bool is_dnd_enabled) {
    controller_->SetDoNotDisturbStateInternal(is_dnd_enabled);
  }

  void RequestNewDoNotDisturbState(bool enabled) {
    controller_->RequestNewDoNotDisturbState(enabled);
  }

  bool GetRecentUpdateNotificationModeRequest() {
    return fake_message_sender_->GetRecentUpdateNotificationModeRequest();
  }

  size_t GetUpdateNotificationModeRequestCallCount() {
    return fake_message_sender_->GetUpdateNotificationModeRequestCallCount();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<FakeMessageSender> fake_message_sender_;
  std::unique_ptr<DoNotDisturbControllerImpl> controller_;
};

TEST_F(DoNotDisturbControllerImplTest, SetInternalStatesWithObservers) {
  EXPECT_FALSE(IsDndEnabled());

  SetDoNotDisturbInternal(/*is_dnd_enabled=*/true);
  EXPECT_TRUE(IsDndEnabled());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Setting internal state with the same previous state will not trigger an
  // observer event.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false);
  EXPECT_FALSE(IsDndEnabled());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(DoNotDisturbControllerImplTest, RequestNewDoNotDisturbState) {
  RequestNewDoNotDisturbState(/*enabled=*/true);
  EXPECT_TRUE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(1u, GetUpdateNotificationModeRequestCallCount());
  // Simulate receiving a response and setting the internal value.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/true);

  RequestNewDoNotDisturbState(/*enabled=*/false);
  EXPECT_FALSE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(2u, GetUpdateNotificationModeRequestCallCount());
  // Simulate receiving a response and setting the internal value.
  SetDoNotDisturbInternal(/*is_dnd_enabled=*/false);

  // Requesting for a the same state as the currently set state is a no-op.
  RequestNewDoNotDisturbState(/*enabled=*/false);
  EXPECT_FALSE(GetRecentUpdateNotificationModeRequest());
  EXPECT_EQ(2u, GetUpdateNotificationModeRequestCallCount());
}

}  // namespace phonehub
}  // namespace chromeos
