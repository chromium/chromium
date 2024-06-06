// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/ui/result_view.h"
#include "chrome/browser/ui/quick_answers/ui/retry_view.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view_utils.h"

namespace quick_answers {
namespace {

constexpr int kMarginDip = 10;
constexpr int kSmallTop = 30;
constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(80, 140));
constexpr char kTestQuery[] = "test-query";

}  // namespace

class QuickAnswersViewsTest : public ChromeQuickAnswersTestBase {
 protected:
  QuickAnswersViewsTest() = default;
  QuickAnswersViewsTest(const QuickAnswersViewsTest&) = delete;
  QuickAnswersViewsTest& operator=(const QuickAnswersViewsTest&) = delete;
  ~QuickAnswersViewsTest() override = default;

  // ChromeQuickAnswersTestBase:
  void SetUp() override {
    ChromeQuickAnswersTestBase::SetUp();

    anchor_bounds_ = kDefaultAnchorBoundsInScreen;
    GetUiController()->GetReadWriteCardsUiController().SetContextMenuBounds(
        anchor_bounds_);
  }

  void TearDown() override { ChromeQuickAnswersTestBase::TearDown(); }

  // Currently instantiated QuickAnswersView instance.
  QuickAnswersView* GetQuickAnswersView() {
    return views::AsViewClass<QuickAnswersView>(
        GetUiController()->quick_answers_view());
  }

  // Needed to poll the current bounds of the mock anchor.
  const gfx::Rect& GetAnchorBounds() { return anchor_bounds_; }

  QuickAnswersUiController* GetUiController() {
    return static_cast<QuickAnswersControllerImpl*>(
               QuickAnswersController::Get())
        ->quick_answers_ui_controller();
  }

  // Create a QuickAnswersView instance with custom anchor-bounds.
  void CreateQuickAnswersView(const gfx::Rect anchor_bounds, bool is_internal) {
    // Set up a companion menu before creating the QuickAnswersView.
    CreateAndShowBasicMenu();

    anchor_bounds_ = anchor_bounds;
    GetUiController()->GetReadWriteCardsUiController().SetContextMenuBounds(
        anchor_bounds_);

    static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get())
        ->SetVisibility(QuickAnswersVisibility::kQuickAnswersVisible);
    // TODO(b/222422130): Rewrite QuickAnswersViewsTest to expand coverage.
    GetUiController()->CreateQuickAnswersView(GetProfile(), "title", kTestQuery,
                                              is_internal);
  }

  void MockGenerateTtsCallback() {
    GetQuickAnswersView()->SetMockGenerateTtsCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::MockGenerateTts,
                            base::Unretained(this)));
  }

  void MockOpenSettingsCallback() {
    GetUiController()->SetFakeOpenSettingsCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::MockOpenSettings,
                            base::Unretained(this)));
  }

  bool is_open_settigns_called() const { return is_open_settings_called_; }

  void MockOpenFeedbackPageCallback() {
    GetUiController()->SetFakeOpenFeedbackPageCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::MockOpenFeedbackPage,
                            base::Unretained(this)));
  }

  std::string mock_feedback_template() const { return mock_feedback_template_; }

  void FakeOnRetryPressed() {
    GetUiController()->SetFakeOnRetryLabelPressedCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::OnRetryPressed,
                            base::Unretained(this)));
  }

  void OnRetryPressed() {
    QuickAnswer quick_answer;
    quick_answer.result_type = ResultType::kDefinitionResult;
    quick_answer.title.push_back(std::make_unique<QuickAnswerText>("Title"));
    quick_answer.first_answer_row.push_back(
        std::make_unique<QuickAnswerText>("FirstAnswerRow"));

    GetUiController()->RenderQuickAnswersViewWithResult(quick_answer);
  }

  PhoneticsInfo mock_phonetics_info() { return mock_phonetics_info_; }

 private:
  void MockGenerateTts(const PhoneticsInfo& phonetics_info) {
    mock_phonetics_info_ = phonetics_info;
  }

  void MockOpenSettings() { is_open_settings_called_ = true; }

  void MockOpenFeedbackPage(const std::string& feedback_template_) {
    mock_feedback_template_ = feedback_template_;
  }

  PhoneticsInfo mock_phonetics_info_;
  bool is_open_settings_called_ = false;
  std::string mock_feedback_template_;
  chromeos::ReadWriteCardsUiController controller_;
  gfx::Rect anchor_bounds_;
};

TEST_F(QuickAnswersViewsTest, DefaultLayoutAroundAnchor) {
  gfx::Rect anchor_bounds = GetAnchorBounds();
  CreateQuickAnswersView(anchor_bounds, /*is_internal=*/false);
  gfx::Rect view_bounds = GetQuickAnswersView()->GetBoundsInScreen();

  // Vertically aligned with anchor.
  EXPECT_EQ(view_bounds.x(), anchor_bounds.x());
  EXPECT_EQ(view_bounds.right(), anchor_bounds.right());

  // View is positioned above the anchor.
  EXPECT_EQ(view_bounds.bottom() + kMarginDip, anchor_bounds.y());
}

TEST_F(QuickAnswersViewsTest, PositionedBelowAnchorIfLessSpaceAbove) {
  gfx::Rect anchor_bounds = GetAnchorBounds();
  // Update anchor-bounds' position so that it does not leave enough vertical
  // space above it to show the QuickAnswersView.
  anchor_bounds.set_y(kSmallTop);

  CreateQuickAnswersView(anchor_bounds, /*is_internal=*/false);
  gfx::Rect view_bounds = GetQuickAnswersView()->GetBoundsInScreen();

  // Anchor is positioned above the view.
  EXPECT_EQ(anchor_bounds.bottom() + kMarginDip, view_bounds.y());
}

TEST_F(QuickAnswersViewsTest, FocusProperties) {
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/false);
  CHECK(views::MenuController::GetActiveInstance() &&
        views::MenuController::GetActiveInstance()->owner());

  // Gains focus only upon request, if an owned menu was active when the view
  // was created.
  CHECK(views::MenuController::GetActiveInstance() != nullptr);
  EXPECT_FALSE(GetQuickAnswersView()->HasFocus());
  GetQuickAnswersView()->RequestFocus();
  EXPECT_TRUE(GetQuickAnswersView()->HasFocus());
}

TEST_F(QuickAnswersViewsTest, Retry) {
  FakeOnRetryPressed();
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/false);

  GetUiController()->ShowRetry();
  RetryView* retry_view = GetQuickAnswersView()->GetRetryViewForTesting();
  ASSERT_TRUE(retry_view);
  EXPECT_TRUE(retry_view->GetVisible());
  GetEventGenerator()->MoveMouseTo(
      retry_view->retry_label_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(GetQuickAnswersView()->GetRetryViewForTesting()->GetVisible());
  EXPECT_TRUE(GetQuickAnswersView()->GetResultViewForTesting()->GetVisible());
}

TEST_F(QuickAnswersViewsTest, Result) {
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/false);

  QuickAnswer quick_answer;
  quick_answer.result_type = ResultType::kDefinitionResult;
  quick_answer.title.push_back(std::make_unique<QuickAnswerText>("Title"));
  quick_answer.first_answer_row.push_back(
      std::make_unique<QuickAnswerText>("FirstAnswerRow"));
  GetUiController()->RenderQuickAnswersViewWithResult(quick_answer);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view);
  EXPECT_TRUE(result_view->GetVisible());
  EXPECT_FALSE(result_view->phonetics_audio_button()->GetVisible());
}

TEST_F(QuickAnswersViewsTest, ResultWithPhoneticsAudio) {
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/false);
  MockGenerateTtsCallback();

  const GURL kTestPhoneticsAudio = GURL("https://example.com/");

  QuickAnswer quick_answer;
  quick_answer.result_type = ResultType::kDefinitionResult;
  quick_answer.title.push_back(std::make_unique<QuickAnswerText>("Title"));
  quick_answer.first_answer_row.push_back(
      std::make_unique<QuickAnswerText>("FirstAnswerRow"));
  quick_answer.phonetics_info.query_text = "QueryText";
  quick_answer.phonetics_info.phonetics_audio = kTestPhoneticsAudio;
  quick_answer.phonetics_info.tts_audio_enabled = true;
  GetUiController()->RenderQuickAnswersViewWithResult(quick_answer);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view);
  EXPECT_TRUE(result_view->GetVisible());
  EXPECT_TRUE(result_view->phonetics_audio_button()->GetVisible());

  GetEventGenerator()->MoveMouseTo(
      result_view->phonetics_audio_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_EQ(mock_phonetics_info().phonetics_audio, kTestPhoneticsAudio);
}

TEST_F(QuickAnswersViewsTest, OpenSettings) {
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/false);
  MockOpenSettingsCallback();

  QuickAnswer quick_answer;
  quick_answer.result_type = ResultType::kDefinitionResult;
  quick_answer.title.push_back(std::make_unique<QuickAnswerText>("Title"));
  quick_answer.first_answer_row.push_back(
      std::make_unique<QuickAnswerText>("FirstAnswerRow"));
  GetUiController()->RenderQuickAnswersViewWithResult(quick_answer);

  GetEventGenerator()->MoveMouseTo(GetQuickAnswersView()
                                       ->GetSettingsButtonForTesting()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(is_open_settigns_called());
  EXPECT_FALSE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, OpenFeedbackPage) {
  CreateQuickAnswersView(GetAnchorBounds(), /*is_internal=*/true);
  MockOpenFeedbackPageCallback();

  QuickAnswer quick_answer;
  quick_answer.result_type = ResultType::kDefinitionResult;
  quick_answer.title.push_back(std::make_unique<QuickAnswerText>("Title"));
  quick_answer.first_answer_row.push_back(
      std::make_unique<QuickAnswerText>("FirstAnswerRow"));
  GetUiController()->RenderQuickAnswersViewWithResult(quick_answer);

  ASSERT_TRUE(
      GetQuickAnswersView()->GetDogfoodButtonForTesting()->GetVisible());
  GetEventGenerator()->MoveMouseTo(GetQuickAnswersView()
                                       ->GetDogfoodButtonForTesting()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_NE(mock_feedback_template().find(kTestQuery), std::string::npos);
  EXPECT_FALSE(GetQuickAnswersView());
}

}  // namespace quick_answers
