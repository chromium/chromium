// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_onboarding_view.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

using ::testing::StrictMock;

class AssistantOnboardingViewTest : public ::testing::Test {
 public:
  AssistantOnboardingViewTest() {
    // Take ownership of the display.
    ON_CALL(display_delegate_, SetView)
        .WillByDefault([&view = view_](std::unique_ptr<views::View> display) {
          view = std::move(display);
          return view.get();
        });

    // Destroy the display if we currently own it.
    ON_CALL(display_delegate_, RemoveView).WillByDefault([&view = view_]() {
      view.reset();
    });
  }
  ~AssistantOnboardingViewTest() override = default;

 protected:
  AssistantOnboardingView* onboarding_view() {
    return static_cast<AssistantOnboardingView*>(view_.get());
  }

  // Mock display delegate and controller.
  StrictMock<MockAssistantDisplayDelegate> display_delegate_;
  StrictMock<MockAssistantOnboardingController> controller_;

  // Variable required to simulate the display delegate.
  std::unique_ptr<views::View> view_;
};

TEST_F(AssistantOnboardingViewTest, CreateAndShowView) {
  // The display delegate is notified that a view wants to register itself.
  EXPECT_CALL(display_delegate_, SetView);

  EXPECT_CALL(controller_, Show);
  controller_.Show(
      AssistantOnboardingPrompt::Create(&controller_, &display_delegate_),
      base::DoNothing());

  // Controller gets notified once the view is destroyed.
  EXPECT_CALL(controller_, OnClose);
  view_.reset();
}

TEST_F(AssistantOnboardingViewTest, CreateShowAndAcceptView) {
  // The display delegate is notified that a view wants to register itself.
  EXPECT_CALL(display_delegate_, SetView);

  EXPECT_CALL(controller_, Show);
  controller_.Show(
      AssistantOnboardingPrompt::Create(&controller_, &display_delegate_),
      base::DoNothing());

  // The controller is notified when the view is accepted.
  EXPECT_CALL(controller_, OnAccept);
  EXPECT_CALL(display_delegate_, RemoveView);
  onboarding_view()->OnAccept();

  // No further calls to the controller take place.
}

TEST_F(AssistantOnboardingViewTest, CreateShowAndCancelView) {
  // The display delegate is notified that a view wants to register itself.
  EXPECT_CALL(display_delegate_, SetView);

  EXPECT_CALL(controller_, Show);
  controller_.Show(
      AssistantOnboardingPrompt::Create(&controller_, &display_delegate_),
      base::DoNothing());

  // The controller is notified when the view is cancelled.
  EXPECT_CALL(controller_, OnCancel);
  EXPECT_CALL(display_delegate_, RemoveView);
  onboarding_view()->OnCancel();

  // No further calls to the controller take place.
}
