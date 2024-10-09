// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

class FedCmAccountSelectionViewBrowserTest : public DialogBrowserTest {
 public:
  FedCmAccountSelectionViewBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kFedCm);
  }

  void PreShow() override {
    delegate_ = std::make_unique<FakeDelegate>(
        browser()->tab_strip_model()->GetActiveWebContents());
    account_selection_view_ = browser()
                                  ->tab_strip_model()
                                  ->GetActiveTab()
                                  ->tab_features()
                                  ->fedcm_account_selection_view_controller()
                                  ->CreateAccountSelectionView(delegate());
  }

  void ShowUi(const std::string& name) override { ShowAccounts(); }

  void ShowAccounts(Account::SignInMode mode = Account::SignInMode::kExplicit) {
    idps_ = {base::MakeRefCounted<content::IdentityProviderData>(
        "idp-example.com", content::IdentityProviderMetadata(),
        content::ClientMetadata(GURL(), GURL(), GURL()),
        blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
        /*has_login_status_mismatch=*/false)};
    accounts_ = {base::MakeRefCounted<Account>(
        "id", "email", "name", "given_name", GURL(),
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>())};
    accounts_[0]->identity_provider = idps_[0];
    account_selection_view()->Show(
        "rp-example.com", idps_, accounts_, mode,
        blink::mojom::RpMode::kPassive,
        /*new_accounts=*/std::vector<IdentityRequestAccountPtr>());
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
  std::vector<IdentityProviderDataPtr> idps_;
  std::vector<IdentityRequestAccountPtr> accounts_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
};

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, CloseAllTabs) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->CloseAllTabs();

  // The dialog should be closed after the WebContents is Hidden.
  ASSERT_FALSE(GetDialog());
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
  EXPECT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // The tab is currently hidden.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->ActivateTabAt(0);

  // The dialog should be reshown after the WebContents is Visible.
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_TRUE(GetDialog()->widget_delegate()->CanActivate());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowWhileHidden) {
  // Run preshow to ensure the dialog will be created in the initial tab.
  PreShow();
  // Add a new tab.
  EXPECT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  ShowUi("");

  // Since Show() was called while hidden, the dialog should have been created,
  // but should not be visible.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       ShowWhileCannotFitInWebContents) {
  browser()->tab_strip_model()->GetActiveWebContents()->Resize(
      gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/10, /*height=*/10));

  Show();
  // Since Show() was called while the web contents is too small, the dialog
  // should have been created, but should not be visible.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       ModalDialogThenShowThenCloseModalDialog) {
  PreShow();
  delegate_->SetAccountSelectedCallback(base::BindOnce(
      &FedCmAccountSelectionViewBrowserTest::ResetAccountSelectionView,
      base::Unretained(this)));
  account_selection_view_->ShowModalDialog(GURL("https://example.test/"),
                                           blink::mojom::RpMode::kPassive);
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

// Tests that adding a new tab hides the FedCM UI, and closing tabs until the
// original tab is shown causes the UI to be reshown.
IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, AddTabHidesUI) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());

  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // The dialog should be hidden since the new tab is appended foregrounded.
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_FALSE(GetDialog()->widget_delegate()->CanActivate());

  ASSERT_TRUE(AddTabAtIndex(2, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_USER_GESTURE);
  ASSERT_TRUE(GetDialog());
  EXPECT_FALSE(GetDialog()->IsVisible());

  browser()->tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_USER_GESTURE);

  // FedCM UI becomes visible again.
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());
}

// Tests that detaching a tab with FedCM UI does not trigger a crash.
IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       DetachTabForInsertion) {
  Show();
  ASSERT_TRUE(GetDialog());
  EXPECT_TRUE(GetDialog()->IsVisible());

  // Add a new tab and detach the FedCM tab without closing it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->DetachTabAtForInsertion(0);

  ASSERT_FALSE(GetDialog());
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
