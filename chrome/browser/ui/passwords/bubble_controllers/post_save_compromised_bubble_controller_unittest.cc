// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/post_save_compromised_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTypeHistogram[] = "PasswordBubble.CompromisedBubble.Type";

using BubbleType = PostSaveCompromisedBubbleController::BubbleType;

class PostSaveCompromisedBubbleControllerTest : public ::testing::Test {
 public:
  PostSaveCompromisedBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~PostSaveCompromisedBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PostSaveCompromisedBubbleController* controller() {
    return controller_.get();
  }

  void CreateController(password_manager::ui::State state);
  void ResetController() { controller_.reset(); }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PostSaveCompromisedBubbleController> controller_;
};

void PostSaveCompromisedBubbleControllerTest::CreateController(
    password_manager::ui::State state) {
  EXPECT_CALL(*delegate(), OnBubbleShown());
  EXPECT_CALL(*delegate(), GetState).WillOnce(testing::Return(state));
  controller_ = std::make_unique<PostSaveCompromisedBubbleController>(
      mock_delegate_->AsWeakPtr());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
}

TEST_F(PostSaveCompromisedBubbleControllerTest, SafeState_Destroy) {
  base::HistogramTester histogram_tester;
  CreateController(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histogram_tester.ExpectUniqueSample(kTypeHistogram,
                                      BubbleType::kPasswordUpdatedSafeState, 1);
}

TEST_F(PostSaveCompromisedBubbleControllerTest, SafeState_DestroyImplicictly) {
  CreateController(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);

  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(PostSaveCompromisedBubbleControllerTest, SafeState_Content) {
  CreateController(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
  EXPECT_EQ(PostSaveCompromisedBubbleController::BubbleType::
                kPasswordUpdatedSafeState,
            controller()->type());
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_NE(gfx::Range(), controller()->GetSettingLinkRange());
  EXPECT_EQ(std::u16string(), controller()->GetButtonText());
  EXPECT_EQ(IDR_SAVED_PASSWORDS_SAFE_STATE_DARK,
            controller()->GetImageID(true));
  EXPECT_EQ(IDR_SAVED_PASSWORDS_SAFE_STATE, controller()->GetImageID(false));
}

TEST_F(PostSaveCompromisedBubbleControllerTest, SafeState_Click) {
  CreateController(password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);

  EXPECT_CALL(*delegate(),
              NavigateToPasswordManagerSettingsPage(
                  password_manager::ManagePasswordsReferrer::kSafeStateBubble));
  controller()->OnSettingsClicked();
}

TEST_F(PostSaveCompromisedBubbleControllerTest, MoreToFix_Destroy) {
  base::HistogramTester histogram_tester;
  CreateController(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histogram_tester.ExpectUniqueSample(
      kTypeHistogram, BubbleType::kPasswordUpdatedWithMoreToFix, 1);
}

TEST_F(PostSaveCompromisedBubbleControllerTest, MoreToFix_DestroyImplicictly) {
  CreateController(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);

  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(PostSaveCompromisedBubbleControllerTest, MoreToFix_Content) {
  CreateController(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
  EXPECT_EQ(PostSaveCompromisedBubbleController::BubbleType::
                kPasswordUpdatedWithMoreToFix,
            controller()->type());
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_EQ(gfx::Range(), controller()->GetSettingLinkRange());
  EXPECT_NE(std::u16string(), controller()->GetButtonText());
  EXPECT_EQ(IDR_SAVED_PASSWORDS_NEUTRAL_STATE_DARK,
            controller()->GetImageID(true));
  EXPECT_EQ(IDR_SAVED_PASSWORDS_NEUTRAL_STATE, controller()->GetImageID(false));
}

TEST_F(PostSaveCompromisedBubbleControllerTest, MoreToFix_Click) {
  base::HistogramTester histogram_tester;
  CreateController(password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);

  EXPECT_CALL(*delegate(),
              NavigateToPasswordCheckup(
                  password_manager::PasswordCheckReferrer::kMoreToFixBubble));
  controller()->OnAccepted();
  ResetController();
}

}  // namespace
