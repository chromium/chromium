// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/one_time_tokens/gmail_otp_opt_in_bubble_view.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/autofill/one_time_tokens/gmail_otp_opt_in_bubble_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using TestParameterType = std::tuple<bool, bool>;

class GmailOtpOptInBubbleViewBrowsertest
    : public UiBrowserTest,
      public testing::WithParamInterface<TestParameterType> {
 public:
  GmailOtpOptInBubbleViewBrowsertest() = default;
  ~GmailOtpOptInBubbleViewBrowsertest() override = default;

  bool IsDarkModeOn() const { return std::get<0>(GetParam()); }
  bool IsBrowserLanguageRTL() const { return std::get<1>(GetParam()); }

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();
    ThemeServiceFactory::GetForProfile(browser()->GetProfile())
        ->UseDefaultTheme();
    scoped_rtl_ = std::make_unique<base::i18n::ScopedRTLForTesting>(
        IsBrowserLanguageRTL());
  }

  void TearDownOnMainThread() override {
    scoped_rtl_.reset();
    UiBrowserTest::TearDownOnMainThread();
  }

  void DismissUi() override {
    GmailOtpOptInBubbleController* controller =
        GmailOtpOptInBubbleController::From(
            *browser()->GetActiveTabInterface());
    if (controller && controller->bubble_view()) {
      static_cast<GmailOtpOptInBubbleView*>(controller->bubble_view())
          ->GetWidget()
          ->CloseNow();
      EXPECT_FALSE(controller->IsShowingBubble());
      EXPECT_TRUE(base::test::RunUntil([this]() {
        return !BrowserElements::From(browser())->GetElement(
            kPageActionGmailOtpOptInElementId);
      }));
    }
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<TestParameterType>& param_info) {
    return base::StrCat(
        {std::get<0>(param_info.param) ? "Dark" : "Light",
         std::get<1>(param_info.param) ? "BrowserRTL" : "BrowserLTR"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn()) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    GmailOtpOptInBubbleController::From(*browser()->GetActiveTabInterface())
        ->SetUpAndShowBubble(u"elisa.g.beckett@gmail.com", base::DoNothing());
  }

  bool VerifyUi() override {
    GmailOtpOptInBubbleController* controller =
        GmailOtpOptInBubbleController::From(
            *browser()->GetActiveTabInterface());
    if (!controller || !controller->bubble_view()) {
      return false;
    }

    if (!base::test::RunUntil([this]() {
          return BrowserElements::From(browser())->GetElement(
                     kPageActionGmailOtpOptInElementId) != nullptr;
        })) {
      return false;
    }

    auto* bubble_view =
        static_cast<GmailOtpOptInBubbleView*>(controller->bubble_view());
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(bubble_view->GetWidget(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  std::unique_ptr<base::i18n::ScopedRTLForTesting> scoped_rtl_;
};

IN_PROC_BROWSER_TEST_P(GmailOtpOptInBubbleViewBrowsertest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         GmailOtpOptInBubbleViewBrowsertest,
                         Combine(/*is_dark_mode=*/Bool(), /*is_rtl=*/Bool()),
                         GmailOtpOptInBubbleViewBrowsertest::GetTestSuffix);

}  // namespace
}  // namespace autofill
