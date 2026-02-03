// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_local_save_notification_view.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/autofill/autofill_ai/mock_autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using TestParameterType = std::tuple<bool, bool>;

class AutofillAiLocalSaveNotificationViewBrowsertest
    : public UiBrowserTest,
      public ::testing::WithParamInterface<TestParameterType> {
 public:
  AutofillAiLocalSaveNotificationViewBrowsertest() = default;
  ~AutofillAiLocalSaveNotificationViewBrowsertest() override = default;

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();
    base::i18n::SetRTLForTesting(IsBrowserLanguageRTL(this->GetParam()));
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
    auto bubble = std::make_unique<AutofillAiLocalSaveNotificationView>(
        nullptr, browser()->tab_strip_model()->GetActiveWebContents(),
        &mock_controller());
    bubble->set_has_parent(false);
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();
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

  MockAutofillAiImportDataController& mock_controller() {
    return mock_controller_;
  }

 private:
  NiceMock<MockAutofillAiImportDataController> mock_controller_;
  raw_ptr<AutofillAiLocalSaveNotificationView> bubble_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(AutofillAiLocalSaveNotificationViewBrowsertest,
                       ShowDialog) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillAiLocalSaveNotificationViewBrowsertest,
    Combine(/*is_dark_mode=*/Bool(), /*is_rtl=*/Bool()),
    AutofillAiLocalSaveNotificationViewBrowsertest::GetTestSuffix);

}  // namespace
}  // namespace autofill
