// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event.h"

namespace webid {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;

class MockDelegate : public AccountSelectionView::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              OnAccountSelected,
              (const GURL&,
               const std::string&,
               const content::IdentityRequestAccount::LoginState&),
              (override));
  MOCK_METHOD(void,
              OnDismiss,
              (content::IdentityRequestDialogController::DismissReason),
              (override));
  MOCK_METHOD(void, OnLoginToIdP, (const GURL&, const GURL&), (override));
  MOCK_METHOD(void, OnMoreDetails, (), (override));
  MOCK_METHOD(void, OnAccountsDisplayed, (), (override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
};

class FedCmAmbientUiBrowserTest : public InProcessBrowserTest {
 public:
  FedCmAmbientUiBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kFedCm, features::kFedCmAmbientUI}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    delegate_ = std::make_unique<NiceMock<MockDelegate>>();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ON_CALL(*delegate_, GetWebContents())
        .WillByDefault(testing::Return(web_contents));
    ON_CALL(*delegate_, GetNativeView())
        .WillByDefault(testing::Return(gfx::NativeView()));

    account_selection_view_ = std::make_unique<FedCmAccountSelectionView>(
        delegate_.get(), browser()->GetActiveTabInterface());
  }

  void TearDownOnMainThread() override {
    if (account_selection_view_) {
      account_selection_view_->Close(/*notify_delegate=*/false,
                                     /*hide_widget=*/false);
    }
    account_selection_view_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void ShowAmbientUi(
      content::IdentityRequestAccount::LoginState browser_login_state,
      std::optional<content::IdentityRequestAccount::LoginState>
          idp_login_state = std::nullopt,
      size_t num_accounts = 1) {
    idps_ = {base::MakeRefCounted<content::IdentityProviderData>(
        "idp-example.com", content::IdentityProviderMetadata(),
        content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
        blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
        std::vector<content::IdentityRequestDialogDisclosureField>(),
        /*has_login_status_mismatch=*/false)};

    accounts_.clear();
    for (size_t i = 0; i < num_accounts; ++i) {
      auto account = base::MakeRefCounted<content::IdentityRequestAccount>(
          "id" + base::NumberToString(i), "display_identifier", "display_name",
          "email", "name", "given_name", GURL(), "tel", "username",
          /*potentially_approved_origin_hashes=*/std::vector<std::string>(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>());
      account->identity_provider = idps_[0];
      account->browser_trusted_login_state = browser_login_state;
      account->idp_claimed_login_state = idp_login_state;

      // Provide a dummy decoded picture to avoid image fetching.
      SkBitmap bitmap;
      bitmap.allocN32Pixels(1, 1);
      bitmap.eraseColor(SK_ColorRED);
      account->decoded_picture = gfx::Image::CreateFrom1xBitmap(bitmap);
      accounts_.push_back(account);
    }

    account_selection_view_->Show(
        content::RelyingPartyData(u"rp-example.com", u""), idps_, accounts_,
        blink::mojom::RpMode::kPassive, {});
  }

  void ShowMultiIdpAmbientUi() {
    idps_ = {base::MakeRefCounted<content::IdentityProviderData>(
                 "idp1.com", content::IdentityProviderMetadata(),
                 content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
                 blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
                 std::vector<content::IdentityRequestDialogDisclosureField>(),
                 /*has_login_status_mismatch=*/false),
             base::MakeRefCounted<content::IdentityProviderData>(
                 "idp2.com", content::IdentityProviderMetadata(),
                 content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
                 blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
                 std::vector<content::IdentityRequestDialogDisclosureField>(),
                 /*has_login_status_mismatch=*/false)};
    accounts_ = {
        base::MakeRefCounted<content::IdentityRequestAccount>(
            "id1", "id1", "name1", "email1", "name1", "given1", GURL(), "tel",
            "user1", std::vector<std::string>(), std::vector<std::string>(),
            std::vector<std::string>(), std::vector<std::string>()),
        base::MakeRefCounted<content::IdentityRequestAccount>(
            "id2", "id2", "name2", "email2", "name2", "given2", GURL(), "tel",
            "user2", std::vector<std::string>(), std::vector<std::string>(),
            std::vector<std::string>(), std::vector<std::string>())};
    accounts_[0]->identity_provider = idps_[0];
    accounts_[1]->identity_provider = idps_[1];

    // Provide a dummy decoded picture to avoid image fetching.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorRED);
    accounts_[0]->decoded_picture = gfx::Image::CreateFrom1xBitmap(bitmap);
    accounts_[1]->decoded_picture = gfx::Image::CreateFrom1xBitmap(bitmap);

    account_selection_view_->Show(
        content::RelyingPartyData(u"rp-example.com", u""), idps_, accounts_,
        blink::mojom::RpMode::kPassive, {});
  }

  FedCmAccountSelectionView* view() { return account_selection_view_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NiceMock<MockDelegate>> delegate_;
  std::vector<IdentityProviderDataPtr> idps_;
  std::vector<IdentityRequestAccountPtr> accounts_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
};

class FedCmAmbientUiDisabledBrowserTest : public FedCmAmbientUiBrowserTest {
 public:
  FedCmAmbientUiDisabledBrowserTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures({features::kFedCm},
                                   {features::kFedCmAmbientUI});
  }
};

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignUpToActiveTransition) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  // Verify chip is showing by checking that we didn't show the dialog widget
  // yet.
  EXPECT_FALSE(view()->GetDialogWidget());
  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);

  // Simulate click on the omnibox chip.
  view()->OnPageActionClicked();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));

  // After activation, the dialog widget should be shown for sign-up (new user).
  EXPECT_TRUE(view()->GetDialogWidget());
  EXPECT_TRUE(view()->GetDialogWidget()->IsVisible());

  // The Page Action icon should still be visible while the modal is shown.
  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);

  // Closing the widget should hide the Page Action icon.
  view()->Close(/*notify_delegate=*/false, /*hide_widget=*/false);
  EXPECT_FALSE(observer.GetCurrentPageActionState().showing);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignInTransition) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Verify chip is showing by checking that we didn't show the dialog widget
  // yet.
  EXPECT_FALSE(view()->GetDialogWidget());

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // Simulate click on the omnibox chip.
  view()->OnPageActionClicked();

  // It should NOT have selected the account yet. It should have opened the
  // anchored message.
  EXPECT_FALSE(account_selected);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  view()->OnPageActionClicked();
  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));

  EXPECT_FALSE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignInCollapsedTransition) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Initially, for sign-in, the suggestion chip should be showing.
  EXPECT_TRUE(observer.GetCurrentPageActionState().chip_showing);

  // We simulate it being collapsed.
  controller->HideSuggestionChip(kActionFederation);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().chip_showing; }));

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // Simulate click on the collapsed omnibox chip.
  view()->OnPageActionClicked();

  // It should NOT have selected the account yet.
  EXPECT_FALSE(account_selected);

  // Instead, it should have re-shown the suggestion chip.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Click on the suggestion chip.
  view()->OnPageActionClicked();

  // It should still NOT have selected the account yet. It should have opened
  // the anchored message.
  EXPECT_FALSE(account_selected);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // Finally, click on the anchored message, now it should select the account.
  view()->OnPageActionClicked();
  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignInAnchoredMessageClick) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Simulate click on the omnibox chip to open the anchored message.
  view()->OnPageActionClicked();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // Find the anchored message view and simulate clicking its button.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* page_action_view = browser_view->GetLocationBarView()
                               ->page_action_container()
                               ->GetPageActionView(kActionFederation);
  ASSERT_TRUE(page_action_view);
  auto* anchored_message = page_action_view->GetAnchoredMessageForTesting();
  ASSERT_TRUE(anchored_message);

  // The confirm button is the 3rd child (index 2) as seen in
  // AnchoredMessageBubbleView constructor.
  views::View* confirm_button = anchored_message->children()[2];
  ASSERT_TRUE(confirm_button);

  // Click the confirm button.
  ui::MouseEvent click(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  confirm_button->OnMousePressed(click);

  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, MultiAccountFallback) {
  // Show passive UI with 2 accounts.
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                /*idp_login_state=*/std::nullopt,
                /*num_accounts=*/2);

  // Currently, passive UI only supports single-account scenarios.
  // Verify that with multiple accounts, it falls through to standard UI
  // (widget shown immediately).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, MultiIdpFallback) {
  ShowMultiIdpAmbientUi();

  // Currently, passive UI only supports single-account/IDP scenarios.
  // Verify that with multiple IDPs, it falls through to standard UI (widget
  // shown immediately).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiDisabledBrowserTest,
                       FeatureDisabledFallback) {
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  // When features::kFedCmAmbientUI is disabled, it should fall through to
  // standard UI (widget shown immediately).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, ReloadPage) {
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  bool dismissed = false;
  EXPECT_CALL(
      *delegate_,
      OnDismiss(
          content::IdentityRequestDialogController::DismissReason::kOther))
      .WillOnce(InvokeWithoutArgs([&]() { dismissed = true; }));

  // Reload the page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  // The FedCM request should be dismissed on navigation/reload.
  ASSERT_TRUE(base::test::RunUntil([&]() { return dismissed; }));
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, TabSwitching) {
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  // Add a new tab and switch to it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // The view should still be there and able to activate.
  view()->OnPageActionClicked();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       IdpMissingApprovedClientsUsesBrowserSignIn) {
  // Browser thinks it is a sign-in, and IdP is silent.
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                /*idp_login_state=*/std::nullopt);

  // Click on the chip.
  view()->OnPageActionClicked();

  // Anchored message should be shown (no widget yet) because we fallback to
  // the browser's sign-in state.
  EXPECT_FALSE(view()->GetDialogWidget());

  auto* features = browser()->GetActiveTabInterface()->GetTabFeatures();
  auto* controller = features->page_action_controller();
  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);
  EXPECT_TRUE(observer.GetCurrentPageActionState().anchored_message_showing);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       IdpClaimedSignUpOverridesBrowserSignIn) {
  // Browser thinks it is a sign-in, but IdP explicitly says sign-up.
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignUp);

  // Click on the chip.
  view()->OnPageActionClicked();

  // The full UI (modal/bubble) should be shown because it's treated as a
  // sign-up.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       IdpClaimedSignInOverridesBrowserSignUp) {
  // Browser thinks it is a sign-up, but IdP says sign-in.
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignIn);

  // Click on the chip.
  view()->OnPageActionClicked();

  // Anchored message should be shown (no widget yet).
  EXPECT_FALSE(view()->GetDialogWidget());

  auto* features = browser()->GetActiveTabInterface()->GetTabFeatures();
  auto* controller = features->page_action_controller();
  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);
  EXPECT_TRUE(observer.GetCurrentPageActionState().anchored_message_showing);
}

}  // namespace webid
