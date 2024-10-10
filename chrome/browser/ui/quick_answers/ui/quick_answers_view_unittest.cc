// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/test/chrome_quick_answers_test_base.h"
#include "chrome/browser/ui/quick_answers/test/mock_quick_answers_client.h"
#include "chrome/browser/ui/quick_answers/ui/result_view.h"
#include "chrome/browser/ui/quick_answers/ui/retry_view.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view_utils.h"

namespace quick_answers {
namespace {

constexpr int kMarginDip = 8;
constexpr int kSmallTop = 30;
constexpr gfx::Rect kDefaultAnchorBoundsInScreen =
    gfx::Rect(gfx::Point(500, 250), gfx::Size(120, 140));
constexpr char kTestQuery[] = "test-query";

// There are U16 variants as it's not allowed to use `base::UTF8ToUTF16` for
// string literals.
constexpr char kWord[] = "Word";
constexpr char kDefinition[] = "Definition";
constexpr char16_t kDefinitionU16[] = u"Definition";
constexpr char kPhoneticsInfoText[] = "PhoneticsInfoText";
constexpr char16_t kDefinitionTitleU16[] = u"Word · /PhoneticsInfoText/";
constexpr char kPhoneticsInfoQueryText[] = "PhoneticsInfoQueryText";
constexpr char kPhoneticsInfoAudioUrl[] = "https://example.com/";
constexpr char kSourceLocaleJaJp[] = "ja-JP";
constexpr char kTextToTranslate[] = "TextToTranslate";
constexpr char16_t kTextToTranslateU16[] = u"TextToTranslate";
constexpr char kTranslatedText[] = "TranslatedText";
constexpr char16_t kTranslatedTextU16[] = u"TranslatedText";
constexpr char kSourceText[] = "SourceText";
constexpr char16_t kSourceTextU16[] = u"SourceText";
constexpr char kResultText[] = "ResultText";
constexpr char16_t kResultTextU16[] = u"ResultText";

class MockSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  MOCK_METHOD(void,
              ShowChromePageForProfile,
              (Profile * profile,
               const GURL& gurl,
               int64_t display_id,
               apps::LaunchCallback callback),
              (override));
};

}  // namespace

// `QuickAnswersViewsTest` will test UI behaviors with layers above
// `QuickAnswersClient`.
//
// Layers:
// UI codes (`QuickAnswersView`, etc)
// `QuickAnswersUiController`
// `QuickAnswersControllerImpl`
// `QuickAnswersClient`
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

    QuickAnswersControllerImpl* controller =
        static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get());
    CHECK(controller);
    std::unique_ptr<MockQuickAnswersClient> mock_quick_answers_client =
        std::make_unique<MockQuickAnswersClient>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            controller);
    mock_quick_answers_client_ = mock_quick_answers_client.get();
    controller->SetClient(std::move(mock_quick_answers_client));
  }

  void TearDown() override {
    // `MockQuickAnswersClient` will be released if the controller gets
    // released. Release a pointer to avoid having a dangling pointer.
    mock_quick_answers_client_ = nullptr;

    ChromeQuickAnswersTestBase::TearDown();
  }

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
  void CreateQuickAnswersView(const gfx::Rect anchor_bounds,
                              std::optional<Intent> intent,
                              QuickAnswersState::FeatureType feature_type,
                              bool is_internal) {
    // Set up a companion menu before creating the QuickAnswersView.
    CreateAndShowBasicMenu();

    anchor_bounds_ = anchor_bounds;
    GetUiController()->GetReadWriteCardsUiController().SetContextMenuBounds(
        anchor_bounds_);

    static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get())
        ->SetVisibility(QuickAnswersVisibility::kQuickAnswersVisible);
    // TODO(b/222422130): Rewrite QuickAnswersViewsTest to expand coverage.
    GetUiController()->CreateQuickAnswersView(
        GetProfile(), "title", kTestQuery, intent, feature_type, is_internal);
  }

  void SendResult(const DefinitionResult& definition_result) {
    std::unique_ptr<StructuredResult> structured_result =
        std::make_unique<StructuredResult>();
    structured_result->definition_result =
        std::make_unique<DefinitionResult>(definition_result);
    SendResult(std::move(structured_result));
  }

  void SendResult(const TranslationResult& translation_result) {
    std::unique_ptr<StructuredResult> structured_result =
        std::make_unique<StructuredResult>();
    structured_result->translation_result =
        std::make_unique<TranslationResult>(translation_result);
    SendResult(std::move(structured_result));
  }

  void SendResult(const UnitConversionResult& unit_conversion_result) {
    std::unique_ptr<StructuredResult> structured_result =
        std::make_unique<StructuredResult>();
    structured_result->unit_conversion_result =
        std::make_unique<UnitConversionResult>(unit_conversion_result);
    SendResult(std::move(structured_result));
  }

  void SendResult(std::unique_ptr<StructuredResult> structured_result) {
    QuickAnswersControllerImpl* controller =
        static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get());
    CHECK(controller);

    std::unique_ptr<QuickAnswersSession> quick_answers_session =
        std::make_unique<QuickAnswersSession>();
    quick_answers_session->structured_result = std::move(structured_result);
    controller->OnQuickAnswerReceived(std::move(quick_answers_session));
  }

  void MockGenerateTtsCallback() {
    GetQuickAnswersView()->SetMockGenerateTtsCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::MockGenerateTts,
                            base::Unretained(this)));
  }

  void MockOpenFeedbackPageCallback() {
    GetUiController()->SetFakeOpenFeedbackPageCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::MockOpenFeedbackPage,
                            base::Unretained(this)));
  }

  std::string mock_feedback_template() const { return mock_feedback_template_; }

  void MockOpenWebUrlCallback() {
    GetUiController()->SetFakeOpenWebUrlForTesting(base::BindRepeating(
        &QuickAnswersViewsTest::MockOpenWebUrl, base::Unretained(this)));
  }

  GURL mock_open_web_url() const { return mock_open_web_url_; }

  void TriggerNetworkError() {
    QuickAnswersControllerImpl* controller =
        static_cast<QuickAnswersControllerImpl*>(QuickAnswersController::Get());
    controller->OnNetworkError();
  }

  void FakeOnRetryPressed() {
    GetUiController()->SetFakeOnRetryLabelPressedCallbackForTesting(
        base::BindRepeating(&QuickAnswersViewsTest::OnRetryPressed,
                            base::Unretained(this)));
  }

  void OnRetryPressed() {
    DefinitionResult definition_result;
    definition_result.word = kWord;
    definition_result.sense.definition = kDefinition;
    SendResult(definition_result);
  }

  PhoneticsInfo mock_phonetics_info() { return mock_phonetics_info_; }

  MockQuickAnswersClient* mock_quick_answers_client() {
    return mock_quick_answers_client_;
  }

 private:
  void MockGenerateTts(const PhoneticsInfo& phonetics_info) {
    mock_phonetics_info_ = phonetics_info;
  }

  void MockOpenFeedbackPage(const std::string& feedback_template_) {
    mock_feedback_template_ = feedback_template_;
  }

  void MockOpenWebUrl(const GURL& url) { mock_open_web_url_ = url; }

  raw_ptr<MockQuickAnswersClient> mock_quick_answers_client_ = nullptr;
  network::TestURLLoaderFactory test_url_loader_factory_;
  PhoneticsInfo mock_phonetics_info_;
  std::string mock_feedback_template_;
  GURL mock_open_web_url_;
  chromeos::ReadWriteCardsUiController controller_;
  gfx::Rect anchor_bounds_;
};

TEST_F(QuickAnswersViewsTest, DefaultLayoutAroundAnchor) {
  gfx::Rect anchor_bounds = GetAnchorBounds();
  CreateQuickAnswersView(anchor_bounds, Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
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

  CreateQuickAnswersView(anchor_bounds, Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  gfx::Rect view_bounds = GetQuickAnswersView()->GetBoundsInScreen();

  // Anchor is positioned above the view.
  EXPECT_EQ(anchor_bounds.bottom() + kMarginDip, view_bounds.y());
}

TEST_F(QuickAnswersViewsTest, FocusProperties) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
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
  // TODO(b/335701090): change this to use `MockQuickAnswersClient` with a fake
  // behavior.
  FakeOnRetryPressed();

  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  TriggerNetworkError();

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
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  SendResult(definition_result);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view);
  EXPECT_TRUE(result_view->GetVisible());
  EXPECT_FALSE(result_view->phonetics_audio_button()->GetVisible());
}

TEST_F(QuickAnswersViewsTest, ResultWithPhoneticsAudio) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  MockGenerateTtsCallback();
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  definition_result.phonetics_info.text = kPhoneticsInfoText;
  definition_result.phonetics_info.query_text = kPhoneticsInfoQueryText;
  definition_result.phonetics_info.phonetics_audio =
      GURL(kPhoneticsInfoAudioUrl);
  definition_result.phonetics_info.tts_audio_enabled = true;
  SendResult(definition_result);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view);
  EXPECT_TRUE(result_view->GetVisible());
  EXPECT_TRUE(result_view->phonetics_audio_button()->GetVisible());
  EXPECT_TRUE(result_view->GetBoundsInScreen().Contains(
      result_view->phonetics_audio_button()->GetBoundsInScreen()))
      << "Phonetics audio button must be inside ResultView";

  GetEventGenerator()->MoveMouseTo(
      result_view->phonetics_audio_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_EQ(mock_phonetics_info().phonetics_audio,
            GURL(kPhoneticsInfoAudioUrl));
  EXPECT_TRUE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, OpenSettings) {
  MockSettingsWindowManager mock_settings_window_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(
      &mock_settings_window_manager);

  EXPECT_CALL(
      mock_settings_window_manager,
      ShowChromePageForProfile(testing::_, testing::_, testing::_, testing::_));

  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  SendResult(definition_result);

  GetEventGenerator()->MoveMouseTo(GetQuickAnswersView()
                                       ->GetSettingsButtonForTesting()
                                       ->GetBoundsInScreen()
                                       .CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, OpenFeedbackPage) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/true);
  MockOpenFeedbackPageCallback();
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  SendResult(definition_result);

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

TEST_F(QuickAnswersViewsTest, ClickResultCard) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  MockOpenWebUrlCallback();
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  SendResult(definition_result);

  GetEventGenerator()->MoveMouseTo(
      GetQuickAnswersView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_EQ(GURL("https://www.google.com/search?q=test-query"),
            mock_open_web_url());
  EXPECT_FALSE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, ClickLoadingCard) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  MockOpenWebUrlCallback();
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  GetEventGenerator()->MoveMouseTo(
      GetQuickAnswersView()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_EQ(GURL("https://www.google.com/search?q=test-query"),
            mock_open_web_url());
  EXPECT_FALSE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, ClickRetryCard) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  MockOpenWebUrlCallback();
  EXPECT_CALL(*mock_quick_answers_client(), OnQuickAnswerClick(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_quick_answers_client(),
              OnQuickAnswersDismissed(testing::_, testing::_))
      .Times(0);

  TriggerNetworkError();

  gfx::Point cursor =
      GetQuickAnswersView()->GetBoundsInScreen().bottom_center();
  cursor.Offset(0, -10);
  ASSERT_FALSE(GetQuickAnswersView()
                   ->GetRetryViewForTesting()
                   ->retry_label_button()
                   ->GetBoundsInScreen()
                   .Contains(cursor))
      << "Make sure to click outside of a retry button";
  ASSERT_TRUE(GetQuickAnswersView()->GetBoundsInScreen().Contains(cursor));
  GetEventGenerator()->MoveMouseTo(cursor);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_EQ(GURL("https://www.google.com/search?q=test-query"),
            mock_open_web_url());
  EXPECT_FALSE(GetQuickAnswersView());
}

TEST_F(QuickAnswersViewsTest, Definition) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  definition_result.phonetics_info.text = kPhoneticsInfoText;
  definition_result.phonetics_info.query_text = kPhoneticsInfoQueryText;
  definition_result.phonetics_info.phonetics_audio =
      GURL(kPhoneticsInfoAudioUrl);
  definition_result.phonetics_info.tts_audio_enabled = true;
  SendResult(definition_result);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view->GetVisible());
  EXPECT_EQ(result_view->GetFirstLineText(), kDefinitionTitleU16);
  EXPECT_TRUE(result_view->GetFirstLineSubText().empty());
  EXPECT_EQ(result_view->GetSecondLineText(), kDefinitionU16);
  EXPECT_TRUE(result_view->phonetics_audio_button()->GetVisible());
}

TEST_F(QuickAnswersViewsTest, Translation) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kTranslation,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  TranslationResult translation_result;
  translation_result.source_locale = kSourceLocaleJaJp;
  translation_result.text_to_translate = kTextToTranslate;
  translation_result.translated_text = kTranslatedText;
  SendResult(translation_result);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view->GetVisible());
  EXPECT_EQ(result_view->GetFirstLineText(), kTextToTranslateU16);
  EXPECT_EQ(result_view->GetFirstLineSubText(),
            chromeos::features::IsQuickAnswersMaterialNextUIEnabled()
                ? u"Japanese"
                : u"");
  EXPECT_EQ(result_view->GetSecondLineText(), kTranslatedTextU16);
}

TEST_F(QuickAnswersViewsTest, UnitConversion) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kUnitConversion,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  UnitConversionResult unit_conversion_result;
  unit_conversion_result.source_text = kSourceText;
  unit_conversion_result.result_text = kResultText;
  SendResult(unit_conversion_result);

  ResultView* result_view = GetQuickAnswersView()->GetResultViewForTesting();
  ASSERT_TRUE(result_view->GetVisible());
  EXPECT_EQ(result_view->GetFirstLineText(), kSourceTextU16);
  EXPECT_EQ(result_view->GetSecondLineText(), kResultTextU16);
}

TEST_F(QuickAnswersViewsTest, IntentTransition) {
  CreateQuickAnswersView(GetAnchorBounds(), /*intent=*/std::nullopt,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);
  EXPECT_EQ(std::nullopt, GetQuickAnswersView()->GetIntent());

  UnitConversionResult unit_conversion_result;
  unit_conversion_result.source_text = kSourceText;
  unit_conversion_result.result_text = kResultText;
  SendResult(unit_conversion_result);
  EXPECT_EQ(Intent::kUnitConversion, GetQuickAnswersView()->GetIntent());
}

TEST_F(QuickAnswersViewsTest, AccessibleProperties) {
  FakeOnRetryPressed();
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  TriggerNetworkError();

  // When RetryView is visible, accessible name for QuickAnswersView should be
  // set to empty.
  RetryView* retry_view = GetQuickAnswersView()->GetRetryViewForTesting();
  EXPECT_TRUE(retry_view->GetVisible());
  ui::AXNodeData data;
  GetQuickAnswersView()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            std::u16string());
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kNameFrom),
            static_cast<int>(ax::mojom::NameFrom::kAttributeExplicitlyEmpty));

  // When RetryView is not visible, accessible name for QuickAnswersView should
  // be set to non-empty pre-determined value.
  retry_view = GetQuickAnswersView()->GetRetryViewForTesting();
  GetEventGenerator()->MoveMouseTo(
      retry_view->retry_label_button()->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(retry_view->GetVisible());
  EXPECT_EQ(GetQuickAnswersView()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT));
}

TEST_F(QuickAnswersViewsTest, AccessibilityDescriptionMagicBoost) {
  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kHmr,
                         /*is_internal=*/false);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  definition_result.phonetics_info.text = kPhoneticsInfoText;
  definition_result.phonetics_info.query_text = kPhoneticsInfoQueryText;
  definition_result.phonetics_info.phonetics_audio =
      GURL(kPhoneticsInfoAudioUrl);
  definition_result.phonetics_info.tts_audio_enabled = true;
  SendResult(definition_result);

  EXPECT_EQ(GetQuickAnswersView()->GetAccessibleDescription(),
            u"Word \xb7 /PhoneticsInfoText/; Definition");
}

TEST_F(QuickAnswersViewsTest, AccessibilityDescriptionRefresh) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kQuickAnswersMaterialNextUI);

  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  DefinitionResult definition_result;
  definition_result.word = kWord;
  definition_result.sense.definition = kDefinition;
  definition_result.phonetics_info.text = kPhoneticsInfoText;
  definition_result.phonetics_info.query_text = kPhoneticsInfoQueryText;
  definition_result.phonetics_info.phonetics_audio =
      GURL(kPhoneticsInfoAudioUrl);
  definition_result.phonetics_info.tts_audio_enabled = true;
  SendResult(definition_result);

  EXPECT_EQ(GetQuickAnswersView()->GetAccessibleDescription(),
            u"Define; Word \xb7 /PhoneticsInfoText/; Definition");
}

TEST_F(QuickAnswersViewsTest, AccessibilityDescriptionSubTextRefresh) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kQuickAnswersMaterialNextUI);

  CreateQuickAnswersView(GetAnchorBounds(), Intent::kDefinition,
                         QuickAnswersState::FeatureType::kQuickAnswers,
                         /*is_internal=*/false);

  TranslationResult translation_result;
  translation_result.source_locale = kSourceLocaleJaJp;
  translation_result.text_to_translate = kTextToTranslate;
  translation_result.translated_text = kTranslatedText;
  SendResult(translation_result);

  EXPECT_EQ(GetQuickAnswersView()->GetAccessibleDescription(),
            u"Translate; TextToTranslate; Japanese; TranslatedText");
}

}  // namespace quick_answers
