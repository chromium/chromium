// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

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
#include "url/gurl.h"

namespace quick_answers {
namespace {

constexpr char kScreenshotPrefix[] = "quick_answers";
constexpr char kTestTitle[] = "TestTitle. A selected text.";
constexpr char kTestQuery[] = "TestQuery";
constexpr char kTestPhoneticsUrl[] = "https://example.com/";
constexpr char kTestDefinition[] =
    "TestDefinition. A test definition for TestTitle.";
constexpr gfx::Rect kContextMenuRectNarrow = {100, 100, 100, 200};
constexpr gfx::Rect kContextMenuRectWide = {100, 100, 300, 200};

using PixelTestParam =
    std::tuple<bool, bool, bool, QuickAnswersView::Design, bool>;

bool IsDarkMode(const PixelTestParam& pixel_test_param) {
  return std::get<0>(pixel_test_param);
}

bool IsRtl(const PixelTestParam& pixel_test_param) {
  return std::get<1>(pixel_test_param);
}

bool IsNarrowLayout(const PixelTestParam& pixel_test_param) {
  return std::get<2>(pixel_test_param);
}

QuickAnswersView::Design GetDesign(const PixelTestParam& pixel_test_param) {
  return std::get<3>(pixel_test_param);
}

bool IsInternal(const PixelTestParam& pixel_test_param) {
  return std::get<4>(pixel_test_param);
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

std::optional<std::string> MaybeGetDesignParamValue(
    const PixelTestParam& pixel_test_param) {
  switch (GetDesign(pixel_test_param)) {
    case QuickAnswersView::Design::kCurrent:
      return std::nullopt;
    case QuickAnswersView::Design::kRefresh:
      return "Refresh";
    case QuickAnswersView::Design::kMagicBoost:
      return "MagicBoost";
  }

  CHECK(false) << "Invalid design enum class value specified";
}

std::optional<std::string> MaybeInternalParamValue(
    const PixelTestParam& pixel_test_param) {
  if (IsInternal(pixel_test_param)) {
    return "Internal";
  }

  return std::nullopt;
}

std::string GetParamName(const PixelTestParam& param,
                         std::string_view separator) {
  std::vector<std::string> param_names;
  param_names.push_back(GetDarkModeParamValue(param));
  param_names.push_back(GetRtlParamValue(param));
  param_names.push_back(GetNarrowLayoutParamValue(param));
  std::optional<std::string> design_param_value =
      MaybeGetDesignParamValue(param);
  if (design_param_value) {
    param_names.push_back(*design_param_value);
  }
  std::optional<std::string> internal_param_value =
      MaybeInternalParamValue(param);
  if (internal_param_value) {
    param_names.push_back(*internal_param_value);
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
class QuickAnswersPixelTestBase
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  void SetUp() override {
    // Make sure kQuickAnswersRichCard is disabled. It might be enabled via
    // fieldtrial_testing_config.
    scoped_feature_list_.InitAndDisableFeature(
        chromeos::features::kQuickAnswersRichCard);

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

using QuickAnswersPixelTest = QuickAnswersPixelTestBase;
using QuickAnswersPixelTestInternal = QuickAnswersPixelTestBase;

INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(QuickAnswersView::Design::kCurrent,
                                     QuickAnswersView::Design::kRefresh,
                                     QuickAnswersView::Design::kMagicBoost),
                     /*is_internal=*/testing::Values(false)),
    &GenerateParamName);

// Separate parameterized test suite for an internal UI to avoid having large
// number of screenshots.
INSTANTIATE_TEST_SUITE_P(
    PixelTest,
    QuickAnswersPixelTestInternal,
    testing::Combine(/*is_dark_mode=*/testing::Values(false),
                     /*is_rtl=*/testing::Values(false),
                     /*is_narrow=*/testing::Values(false),
                     testing::Values(QuickAnswersView::Design::kCurrent,
                                     QuickAnswersView::Design::kRefresh,
                                     QuickAnswersView::Design::kMagicBoost),
                     /*is_internal=*/testing::Values(true)),
    &GenerateParamName);

}  // namespace

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Loading) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersViewForPixelTest(
      browser()->profile(), kTestQuery,
      {
          .title = kTestTitle,
          .design = GetDesign(GetParam()),
          // Spread intent types between tests to have better UI test coverage.
          .intent = QuickAnswersView::Intent::kTranslation,
          .is_internal = IsInternal(GetParam()),
      });
  read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
  views::Widget* widget = read_write_cards_ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Loading", GetParam()), widget->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Result) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersViewForPixelTest(
      browser()->profile(), kTestQuery,
      {
          .title = kTestTitle,
          .design = GetDesign(GetParam()),
          // Spread intent types between tests to have better UI test coverage.
          .intent = QuickAnswersView::Intent::kDefinition,
          .is_internal = IsInternal(GetParam()),
      });
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
  quick_answer.phonetics_info.query_text = kTestQuery;
  quick_answer.phonetics_info.phonetics_audio = GURL(kTestPhoneticsUrl);
  quick_answer.phonetics_info.tts_audio_enabled = true;
  quick_answers_ui_controller->RenderQuickAnswersViewWithResult(quick_answer);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Result", GetParam()), widget->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTest, Retry) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersViewForPixelTest(
      browser()->profile(), kTestQuery,
      {
          .title = kTestTitle,
          .design = GetDesign(GetParam()),
          // Spread intent types between tests to have better UI test coverage.
          .intent = QuickAnswersView::Intent::kUnitConversion,
          .is_internal = IsInternal(GetParam()),
      });
  read_write_cards_ui_controller.SetContextMenuBounds(GetContextMenuRect());
  views::Widget* widget = read_write_cards_ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  QuickAnswersController* quick_answers_controller =
      QuickAnswersController::Get();
  ASSERT_TRUE(quick_answers_controller);
  quick_answers_controller->SetVisibility(
      QuickAnswersVisibility::kQuickAnswersVisible);

  quick_answers_ui_controller->ShowRetry();

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("Retry", GetParam()), widget->GetContentsView()));
}

IN_PROC_BROWSER_TEST_P(QuickAnswersPixelTestInternal, InternalUi) {
  QuickAnswersUiController* quick_answers_ui_controller =
      GetQuickAnswersUiController();
  ASSERT_TRUE(quick_answers_ui_controller);
  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller =
      quick_answers_ui_controller->GetReadWriteCardsUiController();

  quick_answers_ui_controller->CreateQuickAnswersViewForPixelTest(
      browser()->profile(), kTestQuery,
      {
          .title = kTestTitle,
          .design = GetDesign(GetParam()),
          .intent = QuickAnswersView::Intent::kDefinition,
          .is_internal = IsInternal(GetParam()),
      });
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
  quick_answer.phonetics_info.query_text = kTestQuery;
  quick_answer.phonetics_info.phonetics_audio = GURL(kTestPhoneticsUrl);
  quick_answer.phonetics_info.tts_audio_enabled = true;
  quick_answers_ui_controller->RenderQuickAnswersViewWithResult(quick_answer);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("InternalUi", GetParam()), widget->GetContentsView()));
}

}  // namespace quick_answers
