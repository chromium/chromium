// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

class FakeMessageCenterAsh : public ash::MessageCenterAsh {
 public:
  FakeMessageCenterAsh() = default;
  ~FakeMessageCenterAsh() override = default;

  // MessageCenterAsh override:
  void SetQuietMode(bool in_quiet_mode) override {
    NotifyOnQuietModeChanged(in_quiet_mode);
  }
};

}  // namespace

class AppNotificationHandlerTest : public testing::Test {
 public:
  AppNotificationHandlerTest() = default;
  ~AppNotificationHandlerTest() override = default;

  void SetUp() override {
    ash::MessageCenterAsh::SetForTesting(&message_center_ash_);
    handler_ = std::make_unique<AppNotificationHandler>();
  }

  void TearDown() override {
    handler_.reset();
    ash::MessageCenterAsh::SetForTesting(nullptr);
  }

 protected:
  bool GetHandlerQuietModeState() { return handler_->in_quiet_mode_; }

  void SetQuietModeState(bool quiet_mode_enabled) {
    handler_->SetQuietMode(quiet_mode_enabled);
  }

 private:
  std::unique_ptr<AppNotificationHandler> handler_;
  FakeMessageCenterAsh message_center_ash_;
};

// Tests for update of in_quiet_mode_ variable by MessageCenterAsh observer
// OnQuietModeChange() after quiet mode state change between true and false.
TEST_F(AppNotificationHandlerTest, TestOnQuietModeChanged) {
  ash::MessageCenterAsh::Get()->SetQuietMode(true);
  EXPECT_TRUE(GetHandlerQuietModeState());

  ash::MessageCenterAsh::Get()->SetQuietMode(false);
  EXPECT_FALSE(GetHandlerQuietModeState());
}

// Tests for update of in_quiet_mode_ variable after setting state
// with MessageCenterAsh SetQuietMode() true and false.
TEST_F(AppNotificationHandlerTest, TestSetQuietMode) {
  SetQuietModeState(true);
  EXPECT_TRUE(GetHandlerQuietModeState());

  SetQuietModeState(false);
  EXPECT_FALSE(GetHandlerQuietModeState());
}

}  // namespace settings
}  // namespace chromeos
