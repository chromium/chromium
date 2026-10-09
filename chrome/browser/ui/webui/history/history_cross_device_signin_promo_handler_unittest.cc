// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_cross_device_signin_promo_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"
#include "url/gurl.h"

namespace {

namespace promo_mojom = history_cross_device_signin_promo::mojom;

class FakeSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  void ShowSigninUI(Profile* profile,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action,
                    const std::string& extension_name) override {}
  void ShowReauthUI(Profile* profile,
                    const std::string& email,
                    bool enable_sync,
                    signin_metrics::AccessPoint access_point,
                    signin_metrics::PromoAction promo_action) override {}
  void ShowCrossDeviceSigninQrBubble(
      BrowserWindowInterface* browser,
      GURL qr_code_url,
      base::OnceClosure closing_callback,
      CrossDeviceSigninPromoEntryPoint entry_point) override {
    closing_callback_ = std::move(closing_callback);
  }

  void CloseBubble() { std::move(closing_callback_).Run(); }

 private:
  base::OnceClosure closing_callback_;
};

// Forwards the bubble state updates pushed by the handler to a test callback.
class FakePromoPage : public promo_mojom::HistoryCrossDeviceSigninPromoPage {
 public:
  explicit FakePromoPage(base::RepeatingCallback<void(bool)> on_state_changed)
      : on_state_changed_(std::move(on_state_changed)) {}

  mojo::PendingRemote<promo_mojom::HistoryCrossDeviceSigninPromoPage>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // promo_mojom::HistoryCrossDeviceSigninPromoPage:
  void OnPromoBubbleStateChanged(bool is_open) override {
    on_state_changed_.Run(is_open);
  }

 private:
  base::RepeatingCallback<void(bool)> on_state_changed_;
  mojo::Receiver<promo_mojom::HistoryCrossDeviceSigninPromoPage> receiver_{
      this};
};

class HistoryCrossDeviceSigninPromoHandlerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    // `web_contents()` only exists once the harness has been set up.
    handler_ = std::make_unique<HistoryCrossDeviceSigninPromoHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), web_contents());
  }

  void TearDown() override {
    handler_.reset();
    handler_remote_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

 protected:
  HistoryCrossDeviceSigninPromoHandler& handler() { return *handler_; }
  mojo::Remote<promo_mojom::HistoryCrossDeviceSigninPromoHandler>&
  handler_remote() {
    return handler_remote_;
  }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kCrossDeviceSigninFromDesktop};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  mojo::Remote<promo_mojom::HistoryCrossDeviceSigninPromoHandler>
      handler_remote_;
  std::unique_ptr<HistoryCrossDeviceSigninPromoHandler> handler_;
};

TEST_F(HistoryCrossDeviceSigninPromoHandlerTest,
       BubbleStateChangesAreForwardedToPage) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  FakeSigninUiDelegate ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> reset_delegate =
      signin_ui_util::SetSigninUiDelegateForTesting(&ui_delegate);
  testing::NiceMock<MockBrowserWindowInterface> browser_window;
  EXPECT_CALL(browser_window, GetProfile())
      .WillRepeatedly(testing::Return(profile()));

  base::test::TestFuture<bool> is_open;
  FakePromoPage page(is_open.GetRepeatingCallback());
  handler().SetPage(page.BindAndGetRemote());
  // `SetPage()` pushes the initial state even when no bubble is open.
  EXPECT_FALSE(is_open.Take());

  OpenSigninToPhoneQrCodeBubble(&browser_window,
                                CrossDeviceSigninPromoEntryPoint::kProfileMenu,
                                base::DoNothing());
  EXPECT_TRUE(is_open.Take());

  ui_delegate.CloseBubble();
  EXPECT_FALSE(is_open.Take());
}

// Calling `SetPage()` again (e.g. when the card is re-attached) must move
// updates to the new page, without the old page's subscription lingering and
// sending duplicate updates.
TEST_F(HistoryCrossDeviceSigninPromoHandlerTest, SetPageTwiceRebindsPage) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  FakeSigninUiDelegate ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> reset_delegate =
      signin_ui_util::SetSigninUiDelegateForTesting(&ui_delegate);
  testing::NiceMock<MockBrowserWindowInterface> browser_window;
  EXPECT_CALL(browser_window, GetProfile())
      .WillRepeatedly(testing::Return(profile()));

  std::vector<bool> first_page_states;
  FakePromoPage first_page(base::BindLambdaForTesting(
      [&](bool is_open) { first_page_states.push_back(is_open); }));
  handler().SetPage(first_page.BindAndGetRemote());
  std::vector<bool> second_page_states;
  FakePromoPage second_page(base::BindLambdaForTesting(
      [&](bool is_open) { second_page_states.push_back(is_open); }));
  handler().SetPage(second_page.BindAndGetRemote());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !second_page_states.empty(); }));

  OpenSigninToPhoneQrCodeBubble(&browser_window,
                                CrossDeviceSigninPromoEntryPoint::kProfileMenu,
                                base::DoNothing());
  ui_delegate.CloseBubble();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return second_page_states.size() >= 3; }));

  EXPECT_THAT(second_page_states, testing::ElementsAre(false, true, false));
  // The first page was only told the initial state, if anything, before being
  // replaced.
  EXPECT_THAT(first_page_states,
              testing::AnyOf(testing::IsEmpty(), testing::ElementsAre(false)));
}

// Regression test: when no bubble is opened, the callback must still be
// resolved synchronously so the WebUI does not hang awaiting completion.
TEST_F(HistoryCrossDeviceSigninPromoHandlerTest,
       ActionClickResolvesWhenNoBubbleIsOpened) {
  // There is no browser window hosting `web_contents()` in a unit test, so no
  // bubble can be shown.
  base::test::TestFuture<void> action_completed;
  handler().OnPromoCardActionClicked(action_completed.GetCallback());

  EXPECT_TRUE(action_completed.IsReady());
}

// The promo card is never shown while the feature is disabled, so a click can
// only come from a misbehaving renderer. It must report a bad message without
// reaching `OpenSigninToPhoneQrCodeBubble()`, which `CHECK()`s that the
// feature is enabled.
TEST_F(HistoryCrossDeviceSigninPromoHandlerTest,
       ActionClickReportsBadMessageWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kCrossDeviceSigninFromDesktop);

  mojo::test::BadMessageObserver bad_message_observer;
  base::test::TestFuture<void> action_completed;
  handler_remote()->OnPromoCardActionClicked(action_completed.GetCallback());

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "OnPromoCardActionClicked called while "
            "kCrossDeviceSigninFromDesktop is disabled.");
  EXPECT_TRUE(action_completed.Wait());
}

}  // namespace
