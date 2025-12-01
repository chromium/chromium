// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"

#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_factory.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace wallet {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using TestParameterType = std::tuple<bool, bool>;

class MockWalletablePassSaveBubbleController
    : public WalletablePassSaveBubbleController {
 public:
  explicit MockWalletablePassSaveBubbleController(tabs::TabInterface* tab)
      : WalletablePassSaveBubbleController(tab) {}
  ~MockWalletablePassSaveBubbleController() override = default;

  MOCK_METHOD(void, ShowBubble, (), (override));
};

class WalletablePassSaveBubbleViewBrowserTest
    : public UiBrowserTest,
      public testing::WithParamInterface<TestParameterType> {
 public:
  WalletablePassSaveBubbleViewBrowserTest() = default;
  ~WalletablePassSaveBubbleViewBrowserTest() override = default;

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();

    base::i18n::SetRTLForTesting(IsBrowserLanguageRTL(this->GetParam()));
    mock_controller_ = std::make_unique<
        testing::NiceMock<MockWalletablePassSaveBubbleController>>(
        browser()->tab_strip_model()->GetTabAtIndex(0));

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "test@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  void TearDownOnMainThread() override {
    mock_controller_.reset();
    UiBrowserTest::TearDownOnMainThread();
  }

  void DismissUi() override { bubble_ = nullptr; }

  static bool IsDarkModeOn(const TestParameterType& param) {
    return std::get<0>(param);
  }
  static bool IsBrowserLanguageRTL(const TestParameterType& param) {
    return std::get<1>(param);
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<TestParameterType>& param_info) {
    return base::StrCat(
        {IsDarkModeOn(param_info.param) ? "Dark" : "Light",
         IsBrowserLanguageRTL(param_info.param) ? "BrowserRTL" : "BrowserLTR"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn(this->GetParam())) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    bubble_ = WalletablePassBubbleViewFactory::CreateSaveBubbleView(
        browser()->tab_strip_model()->GetActiveWebContents(),
        mock_controller());
  }

  bool VerifyUi() override {
    if (!bubble_) {
      return false;
    }

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(bubble_->GetWidget(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

  MockWalletablePassSaveBubbleController* mock_controller() {
    return mock_controller_.get();
  }

 private:
  raw_ptr<WalletablePassSaveBubbleView> bubble_ = nullptr;
  std::unique_ptr<testing::NiceMock<MockWalletablePassSaveBubbleController>>
      mock_controller_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(WalletablePassSaveBubbleViewBrowserTest, LoyaltyCard) {
  wallet::WalletablePass pass;
  wallet::LoyaltyCard loyalty_card;
  loyalty_card.plan_name = "Walgreens Rewards";
  loyalty_card.issuer_name = "Walgreens";
  loyalty_card.member_id = "123456789";
  pass.pass_data = std::move(loyalty_card);

  mock_controller()->SetUpAndShowSaveBubble(pass, base::DoNothing());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WalletablePassSaveBubbleViewBrowserTest, EventTicket) {
  wallet::WalletablePass pass;
  wallet::EventPass event_pass;
  event_pass.event_name = "LA Dodgers at SF Giants";
  event_pass.event_start_date = "2020-01-01";
  event_pass.issuer_name = "MLB";
  event_pass.venue = "AT&T Park";
  event_pass.issuer_name = "Ticketmaster";
  pass.pass_data = std::move(event_pass);

  mock_controller()->SetUpAndShowSaveBubble(pass, base::DoNothing());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WalletablePassSaveBubbleViewBrowserTest, TransitTicket) {
  wallet::WalletablePass pass;
  wallet::TransitTicket transit_ticket;
  transit_ticket.agency_name = "Metro Transit";
  transit_ticket.origin = "KGX";
  transit_ticket.destination = "YRK";
  transit_ticket.date_of_travel = "2025-12-25";
  pass.pass_data = std::move(transit_ticket);

  mock_controller()->SetUpAndShowSaveBubble(pass, base::DoNothing());
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WalletablePassSaveBubbleViewBrowserTest,
    Combine(/*is_dark_mode=*/Bool(), /*is_rtl=*/Bool()),
    WalletablePassSaveBubbleViewBrowserTest::GetTestSuffix);

}  // namespace
}  // namespace wallet
