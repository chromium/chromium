// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"

namespace quick_answers {
namespace {

constexpr char kScreenshotPrefix[] = "quick_answers";
constexpr char kTestTitle[] = "TestTitle";
constexpr char kTestQuery[] = "TestQuery";
constexpr gfx::Rect kContextMenuRect = {100, 100, 200, 200};

using PixelTestParam = std::tuple<bool, bool>;

bool IsDarkMode(const PixelTestParam& pixel_test_param) {
  return std::get<0>(pixel_test_param);
}

bool IsRtl(const PixelTestParam& pixel_test_param) {
  return std::get<1>(pixel_test_param);
}

std::string GetDarkModeParamValue(const PixelTestParam& pixel_test_param) {
  return IsDarkMode(pixel_test_param) ? "Dark" : "Light";
}

std::string GetRtlParamValue(const PixelTestParam& pixel_test_param) {
  return IsRtl(pixel_test_param) ? "Rtl" : "Ltr";
}

std::string GenerateTestName(
    const testing::TestParamInfo<PixelTestParam>& test_param_info) {
  const PixelTestParam& param = test_param_info.param;
  return base::StrCat(
      {GetDarkModeParamValue(param), "_", GetRtlParamValue(param)});
}

class QuickAnswersPixelTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsRtl(GetParam())) {
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
    }

    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (!command_line->HasSwitch(switches::kVerifyPixels)) {
      GTEST_SKIP() << "A pixel test requires kVerifyPixels flag.";
    }

    pixel_diff_.emplace(kScreenshotPrefix);
  }

  void SetUpOnMainThread() override {
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  // GetScreenshotName returns a test name as a screenshot name. Test name is
  // already parameterized with a test param.
  std::string GetScreenshotName() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    CHECK(test_info);
    return test_info->name();
  }

  QuickAnswersUiController* GetQuickAnswersUiController() {
    QuickAnswersController* controller = QuickAnswersController::Get();
    if (!controller) {
      return nullptr;
    }

    return static_cast<QuickAnswersControllerImpl*>(controller)
        ->quick_answers_ui_controller();
  }

  std::optional<views::ViewSkiaGoldPixelDiff> pixel_diff_;
};

INSTANTIATE_TEST_SUITE_P(PixelTest,
                         QuickAnswersPixelTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         &GenerateTestName);

}  // namespace

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Loading) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersView(
      browser()->profile(), kTestTitle, kTestQuery, /*is_internal=*/false);
  read_write_cards_ui_controller.SetContextMenuBounds(kContextMenuRect);
  views::Widget* widget = read_write_cards_ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(GetScreenshotName(),
                                                 widget->GetContentsView()));
}
}  // namespace quick_answers
