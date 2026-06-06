// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
#include "chrome/browser/ui/webid/identity_ui_utils.h"
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
  MOCK_METHOD(content::IdentityRequestDialogController::PassiveDialogVolume,
              GetPassiveDialogVolume,
              (),
              (const, override));
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
    ON_CALL(*delegate_, GetPassiveDialogVolume())
        .WillByDefault(
            testing::Return(content::IdentityRequestDialogController::
                                PassiveDialogVolume::kDefault));

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

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

  // Verify chip is showing by checking that we didn't show the dialog widget
  // yet.
  EXPECT_FALSE(view()->GetDialogWidget());
  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);

  // Simulate click on the omnibox chip.
  view()->OnPageActionClicked();
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignUpChip, 1);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));

  // After activation, the dialog widget should be shown for sign-up (new user).
  EXPECT_TRUE(view()->GetDialogWidget());
  EXPECT_TRUE(view()->GetDialogWidget()->IsVisible());

  // The Page Action icon should still be visible and expanded while the modal
  // is shown.
  EXPECT_TRUE(observer.GetCurrentPageActionState().showing);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

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

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  // Initially, for sign-in, the suggestion chip should be showing.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);

  // Verify chip is showing by checking that we didn't show the dialog widget
  // yet.
  EXPECT_FALSE(view()->GetDialogWidget());

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // Simulate click on the omnibox chip.
  view()->OnPageActionClicked();

  // It should have selected the account and logged the user in.
  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !observer.GetCurrentPageActionState().anchored_message_showing;
  }));

  // We show a "Signing in ..." message after the user selects the account.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignInChip, 1);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignInCollapsedTransition) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  // Initially, for sign-in, the suggestion chip should be showing.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);

  // We simulate it being collapsed.
  controller->HideSuggestionChip(kActionFederation);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInIcon, 1);

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // We simulate clicking on the icon, which should now sign the user in
  // directly.
  view()->OnPageActionClicked();

  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));

  // And we show a "Signing in ..." message after the user selects the account.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignInIcon, 1);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, MultiAccountFallback) {
  base::HistogramTester histograms;
  // Show passive UI with 2 accounts.
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                /*idp_login_state=*/std::nullopt,
                /*num_accounts=*/2);

  // Currently, passive UI only supports single-account scenarios.
  // Verify that with multiple accounts, it falls through to standard UI
  // (widget shown immediately).
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, MultiIdpFallback) {
  base::HistogramTester histograms;
  ShowMultiIdpAmbientUi();

  // Currently, passive UI only supports single-account/IDP scenarios.
  // Verify that with multiple IDPs, it falls through to standard UI (widget
  // shown immediately).
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiDisabledBrowserTest,
                       FeatureDisabledFallback) {
  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  // When features::kFedCmAmbientUI is disabled, it should fall through to
  // standard UI (widget shown immediately), and AmbientUI metrics should not be
  // recorded.
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, ReloadPage) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

  bool dismissed = false;
  EXPECT_CALL(
      *delegate_,
      OnDismiss(
          content::IdentityRequestDialogController::DismissReason::kOther))
      .WillOnce(InvokeWithoutArgs([&]() { dismissed = true; }));

  // Reload the page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation(web_contents);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  navigation.Wait();

  // The FedCM request should be dismissed on navigation/reload.
  ASSERT_TRUE(base::test::RunUntil([&]() { return dismissed; }));
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, TabSwitching) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

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
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  // Browser thinks it is a sign-in, and IdP is silent.
  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                /*idp_login_state=*/std::nullopt);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);

  // Click on the chip.
  view()->OnPageActionClicked();
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignInChip, 1);

  // Sign-in should be triggered immediately.
  EXPECT_FALSE(view()->GetDialogWidget());

  EXPECT_FALSE(observer.GetCurrentPageActionState().anchored_message_showing);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       IdpClaimedSignUpOverridesBrowserSignIn) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  // Browser thinks it is a sign-in, but IdP explicitly says sign-up.
  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

  // Click on the chip.
  view()->OnPageActionClicked();
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignUpChip, 1);

  // The full UI (modal/bubble) should be shown because it's treated as a
  // sign-up.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       IdpClaimedSignInOverridesBrowserSignUp) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  // Browser thinks it is a sign-up, but IdP says sign-in.
  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignIn);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);

  // Click on the chip.
  view()->OnPageActionClicked();

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignInChip, 1);
}

// This test verifies the UMA metrics logged during the Ambient UI flow for a
// returning (sign-in) user.
//
// The following UMAs were introduced to capture the sign-in user journey:
//
// 1. Blink.FedCm.Ambient.Impression: Logged with value 'kSignInChip' when the
//    omnibox chip appears.
// 2. Blink.FedCm.Ambient.ClickSource:
//    - 'kSignInChip': Logged when the user clicks the sign-in chip.
//    - 'kSignInIcon': Logged when the user clicks the sign-in icon.
//
// The CTR for sign-in can be calculated the following way:
//
// - Percentage of users that see the URL chip and Sign-in:
//             Blink.FedCm.Ambient.ClickSource.kSignInChip /
//             Blink.FedCm.Ambient.Impression.kSignInChip
//
// - Percentage of users that see the URL icon and Sign-in:
//             Blink.FedCm.Ambient.ClickSource.kSignInIcon /
//             Blink.FedCm.Ambient.Impression.kSignInIcon
//
// There are a few additional signals that are collected as part of the
// page action framework and the existing FedCM UMAs:
//
// 1. Blink.FedCm.Status.RequestIdToken: Logged when the flow completes.
// 2. PageActionController.Federation.Chip.CTR2:
//    - 'kShown': Logged when the omnibox chip appears.
//    - 'kClicked': Logged when the user clicks the chip.
//    - Measuring CTR: The generic CTR for the Page Action can be
//      calculated as (Number of 'kClicked' events) / (Number of 'kShown'
//      events).
// 3. PageActionController.ChipTypeShown: Logged with value 'kFederation'.
// 4. Blink.FedCm.AccountsDialogShown: Should be 0 for this flow, as no modal
//    dialog widget is shown (only the Page Action).
IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, CollectsSignInMetrics) {
  base::HistogramTester histograms;

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

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);

  // Check Page Action shown metrics.
  histograms.ExpectBucketCount("PageActionController.ChipTypeShown",
                               PageActionIconType::kFederation, 1);

  // Simulate click on the omnibox chip.
  // Note: We use the real PageActionView and simulate mouse events to ensure
  // that the PageAction framework's metric recording logic is triggered.
  // Simply calling view()->OnPageActionClicked() would bypass the PageAction
  // framework's CTR logging.
  // TODO(crbug.com/493584925): consider using the ui::test::EventGenerator or
  // views::test::InteractionTestUtilSimulatorViews to simulate the events.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* page_action_view = browser_view->GetLocationBarView()
                               ->page_action_container()
                               ->GetPageActionView(kActionFederation);
  ASSERT_TRUE(page_action_view);
  ui::MouseEvent click_event(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  page_action_view->OnEvent(&click_event);
  ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  page_action_view->OnEvent(&release_event);
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignInChip, 1);
  // Re-assert shown metrics after the chip was clicked.
  histograms.ExpectBucketCount("PageActionController.ChipTypeShown",
                               PageActionIconType::kFederation, 1);

  // Blink.FedCm.AccountsDialogShown is currently associated with the widget
  // (modal or bubble), which is *not* shown in this case.
  histograms.ExpectTotalCount("Blink.FedCm.AccountsDialogShown", 0);

  // For sign-in, clicking the chip completes the flow.
  // In a fully integrated browser test (without MockDelegate), we would
  // expect the success status (Blink.FedCm.Status.RequestIdToken) to be
  // recorded in the content layer here.
}

// This test verifies the UMA metrics logged during the Ambient UI flow for a
// new (sign-up) user.
//
// The following UMAs were introduced to capture the sign-up user journey:
//
// 1. Blink.FedCm.Ambient.Impression: Logged with value 'kSignUpChip' when the
//    omnibox chip appears.
// 2. Blink.FedCm.Ambient.ClickSource:
//    - 'kSignUpChip': Logged when the user clicks the sign-in chip. Note that
//    there is no anchored message for sign-up because we use the modal dialog
//    instead.
//
// The CTR for sign-up can be calculated the following way:
//
// - Percentage of users that see the URL chip and Sign-up:
//              Blink.FedCm.Ambient.ClickSource.kSignInAnchoredMessage /
//              Blink.FedCm.Ambient.Impression.kSignUpChip
//
// - Percentage of users that see the URL chip but don't click on it:
//             Blink.FedCm.Ambient.ClickSource.kSignUpAnchoredMessage /
//             Blink.FedCm.Ambient.Impression.kSignUpChip
IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, CollectsSignUpMetrics) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // FedCM dialog shown metric should still be 0 as it's just the chip.
  histograms.ExpectTotalCount("Blink.FedCm.AccountsDialogShown", 0);

  // For sign-up, clicking on the chip should trigger the accounts displayed
  // callback, which in turn records the FedCM dialog shown metric in the
  // content layer.
  EXPECT_CALL(*delegate_, OnAccountsDisplayed);

  // Simulate click on the omnibox chip.
  // Note: We use the real PageActionView and simulate mouse events to ensure
  // that the PageAction framework's metric recording logic is triggered.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* page_action_view = browser_view->GetLocationBarView()
                               ->page_action_container()
                               ->GetPageActionView(kActionFederation);
  ASSERT_TRUE(page_action_view);
  ui::MouseEvent click_event(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  page_action_view->OnEvent(&click_event);
  ui::MouseEvent release_event(
      ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  page_action_view->OnEvent(&release_event);

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.ClickSource",
                               AmbientClick::kSignUpChip, 1);

  // For sign-up, the click shows the dialog widget immediately.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));

  // Note: We don't check Blink.FedCm.AccountsDialogShown here because it is
  // recorded in the content layer by the real delegate, which is mocked in this
  // test. The EXPECT_CALL(*delegate_, OnAccountsDisplayed) above confirms that
  // the view is correctly triggering the callback that leads to the metric
  // being recorded.

  // Select the first account to complete the flow.
  view()->OnAccountSelected(accounts_[0], release_event);

  // In a fully integrated browser test (without MockDelegate), we would
  // expect the success status (Blink.FedCm.Status.RequestIdToken) to be
  // recorded in the content layer here.
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, MultiIdpOneAccountFallback) {
  idps_ = {base::MakeRefCounted<content::IdentityProviderData>(
               "idp1.com", content::IdentityProviderMetadata(),
               content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
               blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
               std::vector<content::IdentityRequestDialogDisclosureField>(),
               /*has_login_status_mismatch=*/true),
           base::MakeRefCounted<content::IdentityProviderData>(
               "idp2.com", content::IdentityProviderMetadata(),
               content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
               blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
               std::vector<content::IdentityRequestDialogDisclosureField>(),
               /*has_login_status_mismatch=*/false)};

  // Only one account from the second IDP.
  auto account = base::MakeRefCounted<content::IdentityRequestAccount>(
      "id1", "email1", "name1", "email1", "name1", "given1", GURL(), "tel",
      "user1", std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>(), std::vector<std::string>());
  account->identity_provider = idps_[1];
  account->browser_trusted_login_state =
      content::IdentityRequestAccount::LoginState::kSignIn;

  // Provide a dummy decoded picture to avoid image fetching.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorRED);
  account->decoded_picture = gfx::Image::CreateFrom1xBitmap(bitmap);

  accounts_ = {account};

  view()->Show(content::RelyingPartyData(u"rp-example.com", u""), idps_,
               accounts_, blink::mojom::RpMode::kPassive, {});

  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  // Verify that with multiple IDPs (even with one account), it falls through to
  // standard UI (widget shown immediately).
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !!view()->GetDialogWidget(); }));
  EXPECT_TRUE(view()->GetDialogWidget());
  EXPECT_FALSE(observer.GetCurrentPageActionState().showing);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       TabSwitchingNoDoubleCounting) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignUp,
                content::IdentityRequestAccount::LoginState::kSignUp);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);

  // Add a new tab and switch to it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Verify that switching back did not trigger an additional impression log.
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignUpChip, 1);
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 1);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest,
                       SignInNoDoubleCountingOverlap) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  // Initially, suggestion chip is shown.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Verify only kSignInChip is logged, and NO kSignInIcon is logged.
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInIcon, 0);

  // Collapse the chip into a static icon.
  controller->HideSuggestionChip(kActionFederation);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !observer.GetCurrentPageActionState().chip_showing; }));

  // Verify that kSignInIcon is logged *only* after collapse, and total
  // impression becomes 2.
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInIcon, 1);
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 2);
}

IN_PROC_BROWSER_TEST_F(FedCmAmbientUiBrowserTest, SignInNoVerifyingImpression) {
  auto* controller = browser()
                         ->GetActiveTabInterface()
                         ->GetTabFeatures()
                         ->page_action_controller();

  page_actions::PageActionObserver observer(kActionFederation);
  observer.RegisterAsPageActionObserver(*controller);

  base::HistogramTester histograms;
  ShowAmbientUi(content::IdentityRequestAccount::LoginState::kSignIn,
                content::IdentityRequestAccount::LoginState::kSignIn);

  // Initially, suggestion chip is shown.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 1);

  bool account_selected = false;
  EXPECT_CALL(*delegate_, OnAccountSelected).WillOnce(InvokeWithoutArgs([&]() {
    account_selected = true;
  }));

  // We simulate clicking on the chip, which should now sign the user in
  // directly and show "Signing in...".
  view()->OnPageActionClicked();

  ASSERT_TRUE(base::test::RunUntil([&]() { return account_selected; }));

  // Verify that the "Signing in..." chip is shown.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer.GetCurrentPageActionState().chip_showing; }));

  // Verify that NO additional impression was logged when the "Signing in..."
  // chip appeared.
  histograms.ExpectBucketCount("Blink.FedCm.Ambient.Impression",
                               AmbientImpression::kSignInChip, 1);
  histograms.ExpectTotalCount("Blink.FedCm.Ambient.Impression", 1);
}

}  // namespace webid
