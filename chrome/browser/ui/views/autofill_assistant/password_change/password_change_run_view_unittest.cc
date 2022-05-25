// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_password_change_run_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using ::testing::StrictMock;

class PasswordChangeRunViewTest : public views::ViewsTestBase {
 public:
  PasswordChangeRunViewTest() {
    // Take ownership of the display.
    ON_CALL(display_delegate_, SetView)
        .WillByDefault([&view = view_](std::unique_ptr<views::View> display) {
          view = std::move(display);
          return view.get();
        });
  }
  ~PasswordChangeRunViewTest() override = default;

 protected:
  // Mock display delegate and controller.
  StrictMock<MockAssistantDisplayDelegate> display_delegate_;
  StrictMock<MockPasswordChangeRunController> controller_;

  // Variable required to simulate the display delegate.
  std::unique_ptr<views::View> view_;
};

TEST_F(PasswordChangeRunViewTest, CreateAndSetInTheProvidedDisplay) {
  // The display delegate is notified that a view wants to register itself.
  EXPECT_CALL(display_delegate_, SetView);

  PasswordChangeRunDisplay::Create(controller_.GetWeakPtr(),
                                   &display_delegate_);
}
