// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
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

  void ShowUi(const std::string& name) override {
    std::vector<content::IdentityRequestAccount> accounts = {
        {"id", "email", "name", "given_name", GURL::EmptyGURL(),
         std::vector<std::string>()}};
    account_selection_view()->Show(
        "top-frame-example.com",
        absl::make_optional<std::string>("iframe-example.com"),
        {{"idp-example.com", accounts, content::IdentityProviderMetadata(),
          content::ClientMetadata(GURL::EmptyGURL(), GURL::EmptyGURL()),
          blink::mojom::RpContext::kSignIn, /*request_permission=*/true}},
        Account::SignInMode::kExplicit, /*show_auto_reauthn_checkbox=*/false);
  }

  void Show() {
    PreShow();
    ShowUi("");
  }

  base::WeakPtr<views::Widget> GetBubble() {
    return account_selection_view_->bubble_widget_;
  }

  FakeDelegate* delegate() { return delegate_.get(); }

  FedCmAccountSelectionView* account_selection_view() {
    return account_selection_view_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
};

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, Hide) {
  Show();
  ASSERT_TRUE(GetBubble());
  EXPECT_TRUE(GetBubble()->IsVisible());
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  // The bubble should be closed after the WebContents is Hidden.
  ASSERT_TRUE(GetBubble());
  EXPECT_FALSE(GetBubble()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_FALSE(GetBubble()->widget_delegate()->CanActivate());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, NavigateAway) {
  Show();
  ASSERT_TRUE(GetBubble());
  EXPECT_TRUE(GetBubble()->IsVisible());
  // Navigate away to a real URL, otherwise it does not seem to work.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  // The bubble should be closed after the browser navigates away from the page.
  EXPECT_FALSE(GetBubble());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ReShow) {
  Show();
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  // The bubble should be closed after the WebContents is Hidden.
  ASSERT_TRUE(GetBubble());
  EXPECT_FALSE(GetBubble()->IsVisible());

  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  // The bubble should be reshown after the WebContents is Visible.
  ASSERT_TRUE(GetBubble());
  EXPECT_TRUE(GetBubble()->IsVisible());
  // Test workaround for http://crbug.com/1367309 where
  // NativeWidgetMac::Activate() ignores views::Widget::IsVisible().
  EXPECT_TRUE(GetBubble()->widget_delegate()->CanActivate());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, ShowWhileHidden) {
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();
  Show();
  // Since Show() was called while hidden, the bubble should have been created,
  // but should not be visible.
  ASSERT_TRUE(GetBubble());
  EXPECT_FALSE(GetBubble()->IsVisible());
  browser()->tab_strip_model()->GetActiveWebContents()->WasShown();
  ASSERT_TRUE(GetBubble());
  EXPECT_TRUE(GetBubble()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest, DetachAndDelete) {
  Show();
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  EXPECT_FALSE(GetBubble());
}

IN_PROC_BROWSER_TEST_F(FedCmAccountSelectionViewBrowserTest,
                       DetachForInsertion) {
  Show();
  browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  // TODO(npm): it would be better if the bubble actually moves with the
  // corresponding tab, instead of being altogether deleted.
  EXPECT_FALSE(GetBubble());
}
