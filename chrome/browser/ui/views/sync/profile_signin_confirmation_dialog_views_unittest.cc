// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"

namespace {

class TestDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  TestDelegate(int* cancels, int* continues, int* signins)
      : cancels_(cancels), continues_(continues), signins_(signins) {}
  ~TestDelegate() override = default;

  void OnCancelSignin() override { (*cancels_)++; }
  void OnContinueSignin() override { (*continues_)++; }
  void OnSigninWithNewProfile() override { (*signins_)++; }

  int* cancels_;
  int* continues_;
  int* signins_;
};

}  // namespace

class ProfileSigninConfirmationDialogTest : public ChromeViewsTestBase {
 public:
  void BuildDialog(
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile = true) {
    auto dialog = std::make_unique<ProfileSigninConfirmationDialogViews>(
        nullptr, "foo@bar.com", std::move(delegate), prompt_for_new_profile);
    weak_dialog_ = dialog.get();

    widget_ = views::DialogDelegate::CreateDialogWidget(dialog.release(),
                                                        GetContext(), nullptr);
    widget_->Show();
  }

  void PressButton(views::Button* button) {
    views::test::WidgetDestroyedWaiter destroy_waiter(widget_);
    // Synthesize both press & release - different platforms have different
    // notions about whether buttons activate on press or on release.
    button->OnKeyPressed(
        ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
    button->OnKeyReleased(
        ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));
    destroy_waiter.Wait();
  }

  ProfileSigninConfirmationDialogViews* weak_dialog_ = nullptr;
  views::Widget* widget_ = nullptr;
  base::test::ScopedFeatureList features_;
};

// Regression test for https://crbug.com/1054866
TEST_F(ProfileSigninConfirmationDialogTest, CloseButtonOnlyCallsDelegateOnce) {
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  // Press the "continue signin" button.
  views::Button* button =
      static_cast<views::Button*>(weak_dialog_->GetExtraView());
  PressButton(button);

  // The delegate should *not* have gotten a call back to OnCancelSignin. If the
  // fix for https://crbug.com/1054866 regresses, either it will, or we'll have
  // crashed above.
  EXPECT_EQ(cancels, 0);
  EXPECT_EQ(continues, 1);
  EXPECT_EQ(signins, 0);
}

// Regression test for https://crbug.com/1091232
TEST_F(ProfileSigninConfirmationDialogTest, CancelButtonOnlyCallsDelegateOnce) {
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  // Press the "Cancel" button.
  views::Button* button = weak_dialog_->GetCancelButton();
  PressButton(button);

  EXPECT_EQ(cancels, 1);
  EXPECT_EQ(continues, 0);
  EXPECT_EQ(signins, 0);
}

// Regression test for https://crbug.com/1091232
TEST_F(ProfileSigninConfirmationDialogTest,
       NewProfileButtonOnlyCallsDelegateOnce) {
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  // Press the "Signin with new profile" button.
  views::Button* button = weak_dialog_->GetOkButton();
  PressButton(button);

  EXPECT_EQ(cancels, 0);
  EXPECT_EQ(continues, 0);
  EXPECT_EQ(signins, 1);
}

// Regression test for https://crbug.com/1091232
TEST_F(ProfileSigninConfirmationDialogTest,
       CancelButtonOnlyCallsDelegateOnceProfileVersion) {
  features_.InitAndEnableFeature(features::kSyncConfirmationUpdatedText);
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  // Press the "Cancel" button.
  views::Button* button = weak_dialog_->GetCancelButton();
  PressButton(button);

  EXPECT_EQ(cancels, 1);
  EXPECT_EQ(continues, 0);
  EXPECT_EQ(signins, 0);
}

TEST_F(ProfileSigninConfirmationDialogTest,
       NewProfileButtonOnlyCallsDelegateOnceWorkProfileVersion) {
  features_.InitAndEnableFeature(features::kSyncConfirmationUpdatedText);
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  // Press the "Signin with new profile" button.
  views::Button* button = weak_dialog_->GetOkButton();
  PressButton(button);

  EXPECT_EQ(cancels, 0);
  EXPECT_EQ(continues, 0);
  EXPECT_EQ(signins, 1);
}

TEST_F(ProfileSigninConfirmationDialogTest,
       OKButtonContinuesForFreshWorkProfileVersion) {
  features_.InitAndEnableFeature(features::kSyncConfirmationUpdatedText);
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate), /*prompt_for_new_profile=*/false);
  widget_->Show();

  // Press the "Signin with new profile" button.
  views::Button* button = weak_dialog_->GetOkButton();
  PressButton(button);

  EXPECT_EQ(cancels, 0);
  EXPECT_EQ(continues, 1);
  EXPECT_EQ(signins, 0);
}

TEST_F(ProfileSigninConfirmationDialogTest, NoExtraViewWorkProfileVersion) {
  features_.InitAndEnableFeature(features::kSyncConfirmationUpdatedText);
  int cancels = 0;
  int continues = 0;
  int signins = 0;
  auto delegate =
      std::make_unique<TestDelegate>(&cancels, &continues, &signins);
  BuildDialog(std::move(delegate));
  widget_->Show();

  EXPECT_EQ(nullptr, weak_dialog_->GetExtraView());

  // Press the "Cancel" button.
  views::Button* button = weak_dialog_->GetCancelButton();
  PressButton(button);

  EXPECT_EQ(cancels, 1);
  EXPECT_EQ(continues, 0);
  EXPECT_EQ(signins, 0);
}
