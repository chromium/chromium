// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"

namespace quick_answers {
namespace {

constexpr char kScreenshotPrefix[] = "quick_answers";
constexpr char kTestTitle[] = "TestTitle. A selected text.";
constexpr char kTestQuery[] = "TestQuery";
constexpr char kTestDefinition[] =
    "TestDefinition. A test definition for TestTitle.";
constexpr gfx::Rect kContextMenuRectNarrow = {100, 100, 100, 200};
constexpr gfx::Rect kContextMenuRectWide = {100, 100, 300, 200};

using PixelTestParam = std::tuple<bool, bool, bool, bool>;

bool IsDarkMode(const PixelTestParam& pixel_test_param) {
  return std::get<0>(pixel_test_param);
}

bool IsRtl(const PixelTestParam& pixel_test_param) {
  return std::get<1>(pixel_test_param);
}

bool IsNarrowLayout(const PixelTestParam& pixel_test_param) {
  return std::get<2>(pixel_test_param);
}

bool IsRichAnswersEnabled(const PixelTestParam& pixel_test_param) {
  return std::get<3>(pixel_test_param);
}

std::string GetDarkModeParamValue(const PixelTestParam& pixel_test_param) {
  return IsDarkMode(pixel_test_param) ? "Dark" : "Light";
}

std::string GetRtlParamValue(const PixelTestParam& pixel_test_param) {
  return IsRtl(pixel_test_param) ? "Rtl" : "Ltr";
}

std::string GetNarrowLayoutParamValue(const PixelTestParam& pixel_test_param) {
  return IsNarrowLayout(pixel_test_param) ? "Narrow" : "Wide";
}

// RichAnswers variant is removed once rich answers is enabled by default.
std::optional<std::string> MaybeGetRichAnswersParamValue(
    const PixelTestParam& pixel_test_param) {
  if (IsRichAnswersEnabled(pixel_test_param)) {
    return "RichAnswers";
  }
  return std::nullopt;
}

std::string GetParamName(const PixelTestParam& param,
                         std::string_view separator) {
  std::vector<std::string> param_names;
  param_names.push_back(GetDarkModeParamValue(param));
  param_names.push_back(GetRtlParamValue(param));
  param_names.push_back(GetNarrowLayoutParamValue(param));
  std::optional<std::string> rich_answers_param_value =
      MaybeGetRichAnswersParamValue(param);
  if (rich_answers_param_value) {
    param_names.push_back(*rich_answers_param_value);
  }
  return base::JoinString(param_names, separator);
}

std::string GenerateParamName(
    const testing::TestParamInfo<PixelTestParam>& test_param_info) {
  return GetParamName(test_param_info.param, /*separator=*/"");
}

std::string GetScreenshotName(const std::string& test_name,
                              const PixelTestParam& param) {
  return test_name + "." + GetParamName(param, /*separator=*/".");
}

// To run a pixel test locally:
//
// e.g., for only running light mode variants of Loading case:
// browser_tests --gtest_filter=*QuickAnswersPixelTest.Loading/Light*
//   --enable-pixel-output-in-tests
//   --browser-ui-tests-verify-pixels
//   --skia-gold-local-png-write-directory=/tmp/qa_pixel_test
class QuickAnswersPixelTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  void SetUp() override {
    if (IsRichAnswersEnabled(GetParam())) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kQuickAnswersRichCard);
    }

    InProcessBrowserTest::SetUp();
  }

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
  gfx::Rect GetContextMenuRect() {
    return IsNarrowLayout(GetParam()) ? kContextMenuRectNarrow
                                      : kContextMenuRectWide;
  }

  QuickAnswersUiController* GetQuickAnswersUiController() {
    QuickAnswersController* controller = QuickAnswersController::Get();
    if (!controller) {
      return nullptr;
    }

    return static_cast<QuickAnswersControllerImpl*>(controller)
        ->quick_answers_ui_controller();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<views::ViewSkiaGoldPixelDiff> pixel_diff_;
};

INSTANTIATE_TEST_SUITE_P(PixelTest,
                         QuickAnswersPixelTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()),
                         &GenerateParamName);

}  // namespace

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Loading) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersView(
      browser()->profile(), kTestTitle, kTestQuery, /*is_internal=*/false);
  read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
  views::Widget* widget = read_write_cards_ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Loading", GetParam()), widget->GetContentsView()));
}

// TODO(b/331271987): layout is known to be broken for width change now.
IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Result) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersView(
      browser()->profile(), kTestTitle, kTestQuery, /*is_internal=*/false);
  read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
  views::Widget* widget = read_write_cards_ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  QuickAnswersController* quick_answers_controller =
      QuickAnswersController::Get();
  ASSERT_TRUE(quick_answers_controller);
  quick_answers_controller->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);

  QuickAnswer quick_answer;
  quick_answer.result_type = ResultType::kDefinitionResult;
  quick_answer.title.push_back(std::make_unique<QuickAnswerText>(kTestTitle));
  quick_answer.first_answer_row.push_back(
      std::make_unique<QuickAnswerText>(kTestDefinition));
  quick_answers_ui_controller->RenderQuickAnswersViewWithResult(quick_answer);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Result", GetParam()), widget->GetContentsView()));
}

}  // namespace quick_answers
