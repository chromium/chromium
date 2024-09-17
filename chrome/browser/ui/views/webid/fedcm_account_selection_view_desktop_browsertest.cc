// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class FedCmAccountSelectionViewBrowserTest : public DialogBrowserTest {
 public:
  FedCmAccountSelectionViewBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFedCm);
  }

  void PreShow() override {
    delegate_ = std::make_unique<FakeDelegate>(
        browser()->tab_strip_model()->GetActiveWebContents());
    account_selection_view_ =
        std::make_unique<FedCmAccountSelectionView>(delegate());
  }

  void ShowUi(const std::string& name) override { ShowAccounts(); }

  void ShowAccounts(Account::SignInMode mode = Account::SignInMode::kExplicit) {
    std::vector<content::IdentityRequestAccount> accounts = {
        {"id", "email", "name", "given_name", GURL(),
         /*login_hints=*/std::vector<std::string>(),
         /*domain_hints=*/std::vector<std::string>(),
         /*labels=*/std::vector<std::string>()}};
    account_selection_view()->Show(
        "top-frame-example.com",
        std::make_optional<std::string>("iframe-example.com"),
        {{"idp-example.com", accounts, content::IdentityProviderMetadata(),
          content::ClientMetadata(GURL(), GURL(), GURL()),
          blink::mojom::RpContext::kSignIn, /*request_permission=*/true,
          /*has_login_status_mismatch=*/false}},
        mode, blink::mojom::RpMode::kWidget,
        /*new_account_idp*/ std::nullopt);
  }

  void Show() {
    PreShow();
    ShowUi("");
  }

  base::WeakPtr<views::Widget> GetDialog() {
    return account_selection_view_->GetDialogWidget();
  }

  FakeDelegate* delegate() { return delegate_.get(); }

  FedCmAccountSelectionView* account_selection_view() {
    return account_selection_view_.get();
  }

  void ResetAccountSelectionView() { account_selection_view_ = nullptr; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
};

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, Hide) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  // The dialog should be closed after the WebContents is Hidden.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_FALSE(GetDialog()->widget_delegate()->CanActivate());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, NavigateAway) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
  // Navigate away to a real URL, otherwise it does not seem to work.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  // The dialog should be closed after the browser navigates away from the page.
  EXPECT_FALSE(GetDialog());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ReShow) {
  Show();
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  // The dialog should be closed after the WebContents is Hidden.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  // The dialog should be reshown after the WebContents is Visible.
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_TRUE(GetDialog()->widget_delegate()->CanActivate());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowWhileHidden) {
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  Show();
  // Since Show() was called while hidden, the dialog should have been created,
  // but should not be visible.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       ModalDialogThenShowThenCloseModalDialog) {
  PreShow();
  delegate_->SetAccountSelectedCallback(base::BindOnce(
      &FedCmAccountSelectionViewBrowserTest::ResetAccountSelectionView,
      base::Unretained(this)));
  account_selection_view_->ShowModalDialog(GURL("https://example.test/"));
  // Because a modal dialog is up, this should save the accounts for later.
  ShowAccounts(Account::SignInMode::kAuto);
  // This should trigger auto re-authn without crashing or UAF.
  account_selection_view_->CloseModalDialog();
  // The account selected callback should have been called, thus the view
  // should be null now.
  EXPECT_EQ(nullptr, account_selection_view_);
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, DetachAndDelete) {
  Show();
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  EXPECT_FALSE(GetDialog());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       DetachForInsertion) {
  Show();
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  // TODO(npm): it would be better if the dialog actually moves with the
  // corresponding tab, instead of being altogether deleted.
  EXPECT_FALSE(GetDialog());
}

// Tests crash scenario from crbug.com/1473691.
IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ClosedBrowser) {
  PreShow();
  browser()->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser());

  // Invoking this after browser is closed should not cause a crash.
  ShowUi("");
  EXPECT_FALSE(GetDialog());
}

// Test that the dialog is disabled when occluded by a PiP window.
IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       DisabledWhenOccluded) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());

  views::View* dialog_view = GetDialog()->GetContentsView();
  ASSERT_NE(dialog_view, nullptr);

  // Create a picture-in-picture widget that does not occlude the prompt.
  gfx::Rect prompt_widget_bounds =
      dialog_view->GetWidget()->GetWindowBoundsInScreen();
  gfx::Rect non_occluding_bounds =
      gfx::Rect(prompt_widget_bounds.right() + 1, 0, 100, 100);
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = non_occluding_bounds;
  auto pip_widget = std::make_unique<views::Widget>(std::move(init_params));
  pip_widget->Show();
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->OnPictureInPictureWidgetOpened(pip_widget.get());

  // The prompt should be enabled, as it's not occluded.
  EXPECT_TRUE(dialog_view->GetEnabled());

  // Move the picture-in-picture window to occlude the prompt.
  pip_widget->SetBounds(prompt_widget_bounds);

  // The prompt should be disabled. We may need to wait for that to happen.
  if (dialog_view->GetEnabled()) {
    base::RunLoop wait_loop;
    auto subscription =
        dialog_view->AddEnabledChangedCallback(wait_loop.QuitClosure());
    wait_loop.Run();
  }
  EXPECT_FALSE(dialog_view->GetEnabled());

  // Move the picture-in-picture window to no longer occlude the prompt.
  pip_widget->SetBounds(non_occluding_bounds);

  // The prompt should be enabled again. We may need to wait for that to happen.
  if (!dialog_view->GetEnabled()) {
    base::RunLoop wait_loop;
    auto subscription =
        dialog_view->AddEnabledChangedCallback(wait_loop.QuitClosure());
    wait_loop.Run();
  }
  EXPECT_TRUE(dialog_view->GetEnabled());
}
