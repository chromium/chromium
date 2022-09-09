// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller_impl.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_onboarding_prompt.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::StrictMock;

namespace {
constexpr int kConfirmationId = 123;
constexpr int kDescriptionId1 = 37;
constexpr int kDescriptionId2 = 43;
}  // namespace

class AssistantOnboardingControllerImplTest : public ::testing::Test {
 public:
  AssistantOnboardingControllerImplTest() {
    // Create controller without `WebContents` here - the version with
    // `WebContents` is tested in the browsertest.
    controller_ = AssistantOnboardingController::Create(
        AssistantOnboardingInformation(), /*web_contents=*/nullptr);
  }
  ~AssistantOnboardingControllerImplTest() override = default;

 protected:
  // The controller to test.
  std::unique_ptr<AssistantOnboardingController> controller_;
};

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndAccept) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(prompt.GetWeakPtr(), callback.Get());

  // Simulate click on accept.
  EXPECT_CALL(callback,
              Run(true, absl::optional<int>(kConfirmationId),
                  std::vector<int>({kDescriptionId1, kDescriptionId2})));
  controller_->OnAccept(kConfirmationId, {kDescriptionId1, kDescriptionId2});
}

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndCancel) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(prompt.GetWeakPtr(), callback.Get());

  // Simulate click on cancel.
  EXPECT_CALL(callback, Run(false, absl::optional<int>(), std::vector<int>()));
  controller_->OnCancel();
}

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndClose) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(prompt.GetWeakPtr(), callback.Get());

  // Simulate click on cancel.
  EXPECT_CALL(callback, Run(false, absl::optional<int>(), std::vector<int>()));
  controller_->OnClose();

  // A second call does not do anything.
  controller_->OnClose();
}

TEST_F(AssistantOnboardingControllerImplTest, ShowTwoPromptsAndAcceptSecond) {
  StrictMock<MockAssistantOnboardingPrompt> first_prompt;
  base::MockCallback<AssistantOnboardingController::Callback> first_callback;

  EXPECT_CALL(first_prompt, Show);
  controller_->Show(first_prompt.GetWeakPtr(), first_callback.Get());

  StrictMock<MockAssistantOnboardingPrompt> second_prompt;
  base::MockCallback<AssistantOnboardingController::Callback> second_callback;

  // The second prompt closes the first.
  EXPECT_CALL(first_prompt, OnControllerGone);
  EXPECT_CALL(first_callback,
              Run(false, absl::optional<int>(), std::vector<int>()));
  EXPECT_CALL(second_prompt, Show);
  controller_->Show(second_prompt.GetWeakPtr(), second_callback.Get());

  // Simulate click on accept.
  EXPECT_CALL(second_callback, Run(true, absl::optional<int>(kConfirmationId),
                                   std::vector<int>({kDescriptionId1})));
  controller_->OnAccept(kConfirmationId, {kDescriptionId1});
}

TEST_F(AssistantOnboardingControllerImplTest, ShowPromptAndRemoveController) {
  StrictMock<MockAssistantOnboardingPrompt> prompt;
  base::MockCallback<AssistantOnboardingController::Callback> callback;

  EXPECT_CALL(prompt, Show);
  controller_->Show(prompt.GetWeakPtr(), callback.Get());

  // Destroying the controller should notify the prompt and run the callback.
  EXPECT_CALL(prompt, OnControllerGone);
  EXPECT_CALL(callback, Run(false, absl::optional<int>(), std::vector<int>()));
  controller_.reset();
}
