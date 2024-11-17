// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/translate/core/browser/mock_translate_metrics_logger.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

namespace {

class FakeTranslateBubbleModel : public TranslateBubbleModel {
 public:
  explicit FakeTranslateBubbleModel(
      TranslateBubbleModel::ViewState view_state) {
    DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
    DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
    current_view_state_ = view_state;
  }

  TranslateBubbleModel::ViewState GetViewState() const override {
    return current_view_state_;
  }

  void SetViewState(TranslateBubbleModel::ViewState view_state) override {
    current_view_state_ = view_state;
  }

  void ShowError(translate::TranslateErrors error_type) override {}

  int GetNumberOfSourceLanguages() const override { return 1000; }

  int GetNumberOfTargetLanguages() const override { return 1000; }

  std::u16string GetSourceLanguageNameAt(int index) const override {
    return u"English";
  }

  std::u16string GetTargetLanguageNameAt(int index) const override {
    return u"English";
  }

  std::string GetSourceLanguageCode() const override { return "eng-US"; }

  int GetSourceLanguageIndex() const override { return 1; }

  void UpdateSourceLanguageIndex(int index) override {}

  int GetTargetLanguageIndex() const override { return 2; }

  void UpdateTargetLanguageIndex(int index) override {}

  void DeclineTranslation() override {}

  bool ShouldNeverTranslateLanguage() override { return false; }

  void SetNeverTranslateLanguage(bool value) override {}

  bool ShouldNeverTranslateSite() override { return false; }

  void SetNeverTranslateSite(bool value) override {}

  bool ShouldAlwaysTranslateBeCheckedByDefault() const override {
    return false;
  }

  bool ShouldShowAlwaysTranslateShortcut() const override { return false; }

  bool ShouldAlwaysTranslate() const override { return false; }

  void SetAlwaysTranslate(bool value) override {}

  void Translate() override {}

  void RevertTranslation() override {}

  void OnBubbleClosing() override {}

  bool IsPageTranslatedInCurrentLanguages() const override { return false; }

  bool CanAddSiteToNeverPromptList() override { return false; }

  void ReportUIInteraction(translate::UIInteraction ui_interaction) override {}

  void ReportUIChange(bool is_ui_shown) override {}

  ViewState current_view_state_;
};

class FakePartialTranslateBubbleModel : public PartialTranslateBubbleModel {
 public:
  explicit FakePartialTranslateBubbleModel(
      PartialTranslateBubbleModel::ViewState view_state) {
    DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
    DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
    current_view_state_ = view_state;
    error_ = translate::TranslateErrors::NONE;
  }

  void AddObserver(PartialTranslateBubbleModel::Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(
      PartialTranslateBubbleModel::Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  PartialTranslateBubbleModel::ViewState GetViewState() const override {
    return current_view_state_;
  }

  void SetViewState(
      PartialTranslateBubbleModel::ViewState view_state) override {
    current_view_state_ = view_state;
  }

  void SetSourceLanguage(const std::string& language_code) override {}
  void SetTargetLanguage(const std::string& language_code) override {}

  void SetSourceText(const std::u16string& text) override {}
  std::u16string GetSourceText() const override { return u"source"; }
  void SetTargetText(const std::u16string& text) override {}
  std::u16string GetTargetText() const override { return u"target"; }

  void SetError(translate::TranslateErrors error_type) override {
    error_ = error_type;
  }
  translate::TranslateErrors GetError() const override { return error_; }

  int GetNumberOfSourceLanguages() const override { return 1000; }

  int GetNumberOfTargetLanguages() const override { return 1000; }

  std::u16string GetSourceLanguageNameAt(int index) const override {
    return u"English";
  }

  std::u16string GetTargetLanguageNameAt(int index) const override {
    return u"English";
  }

  int GetSourceLanguageIndex() const override { return 1; }

  void UpdateSourceLanguageIndex(int index) override {}

  int GetTargetLanguageIndex() const override { return 2; }

  void UpdateTargetLanguageIndex(int index) override {}

  std::string GetSourceLanguageCode() const override { return "en"; }

  std::string GetTargetLanguageCode() const override { return "en"; }

  void Translate(content::WebContents* web_contents) override {}

  void TranslateFullPage(content::WebContents* web_contents) override {}

  void SetSourceTextTruncated(bool is_truncated) override {
    source_text_truncated_ = is_truncated;
  }

  bool GetSourceTextTruncatedForTest() { return source_text_truncated_; }

  void NotifyTranslated() {
    observers_.Notify(
        &PartialTranslateBubbleModel::Observer::OnPartialTranslateComplete);
  }

  ViewState current_view_state_;
  translate::TranslateErrors error_;

 private:
  bool source_text_truncated_;
  base::ObserverList<PartialTranslateBubbleModel::Observer> observers_;
};

}  // namespace

class TranslateBubbleControllerTest : public ChromeViewsTestBase {
 public:
  TranslateBubbleControllerTest() = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // Owned by WebContents.
    controller_ = new TranslateBubbleController(web_contents_.get());
    web_contents_->SetUserData(TranslateBubbleController::UserDataKey(),
                               base::WrapUnique(controller_.get()));

    // Use fake Translate bubble models instead of real implementations for
    // Translate bubble view construction in tests.
    controller_->SetTranslateBubbleModelFactory(base::BindRepeating(
        &TranslateBubbleControllerTest::CreateFakeTranslateBubbleModel,
        base::Unretained(this)));
    controller_->SetPartialTranslateBubbleModelFactory(base::BindRepeating(
        &TranslateBubbleControllerTest::CreateFakePartialTranslateBubbleModel,
        base::Unretained(this)));
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<TranslateBubbleModel> CreateFakeTranslateBubbleModel() {
    auto model = std::make_unique<FakeTranslateBubbleModel>(
        TranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE);
    fake_translate_bubble_model_ = model.get();
    return model;
  }

  std::unique_ptr<PartialTranslateBubbleModel>
  CreateFakePartialTranslateBubbleModel() {
    auto model = std::make_unique<FakePartialTranslateBubbleModel>(
        PartialTranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE);
    fake_partial_translate_bubble_model_ = model.get();
    return model;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<translate::testing::MockTranslateMetricsLogger>
      mock_translate_metrics_logger_;

  raw_ptr<FakeTranslateBubbleModel, DanglingUntriaged>
      fake_translate_bubble_model_ = nullptr;
  raw_ptr<FakePartialTranslateBubbleModel, DanglingUntriaged>
      fake_partial_translate_bubble_model_ = nullptr;

  // Owned by WebContents.
  raw_ptr<TranslateBubbleController> controller_;
};

TEST_F(TranslateBubbleControllerTest, ShowFullPageThenPartialTranslateBubble) {
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());

  // Show the Full Page Translate bubble first.
  controller_->ShowTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
      translate::TranslateErrors::NONE,
      LocationBarBubbleDelegateView::DisplayReason::AUTOMATIC);

  EXPECT_THAT(controller_->GetTranslateBubble(), testing::NotNull());

  // Starting a Partial Translate while the Full Page Translate bubble is open
  // should close the Full Page Translate bubble.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  fake_partial_translate_bubble_model_->NotifyTranslated();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::NotNull());
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());

  // Only the Partial Translate bubble should remain, close it.
  controller_->CloseBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());
}

TEST_F(TranslateBubbleControllerTest, ShowPartialThenFullPageTranslateBubble) {
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());

  // Show the Partial Translate bubble first.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  fake_partial_translate_bubble_model_->NotifyTranslated();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::NotNull());

  // Showing the Full Page Translate bubble while the Partial Translate bubble
  // is open should close the Partial Translate bubble.
  controller_->ShowTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
      translate::TranslateErrors::NONE,
      LocationBarBubbleDelegateView::DisplayReason::AUTOMATIC);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::NotNull());
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());

  // Only the Full Page Translate bubble should remain, close it.
  controller_->CloseBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());
}

TEST_F(TranslateBubbleControllerTest, PartialTranslateTimerExpired) {
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  task_environment()->FastForwardBy(base::Milliseconds(10));
  ASSERT_TRUE(controller_->GetPartialTranslateBubble());
  EXPECT_FALSE(
      controller_->GetPartialTranslateBubble()->GetWidget()->IsVisible());

  task_environment()->FastForwardBy(base::Milliseconds(
      translate::kDesktopPartialTranslateBubbleShowDelayMs + 100));
  EXPECT_TRUE(
      controller_->GetPartialTranslateBubble()->GetWidget()->IsVisible());
  EXPECT_EQ(fake_partial_translate_bubble_model_->GetViewState(),
            PartialTranslateBubbleModel::ViewState::VIEW_STATE_WAITING);

  fake_partial_translate_bubble_model_->NotifyTranslated();
  EXPECT_EQ(fake_partial_translate_bubble_model_->GetViewState(),
            PartialTranslateBubbleModel::ViewState::VIEW_STATE_AFTER_TRANSLATE);
}

TEST_F(TranslateBubbleControllerTest, PartialTranslateResponseBeforeTimer) {
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  task_environment()->FastForwardBy(base::Milliseconds(10));
  ASSERT_TRUE(controller_->GetPartialTranslateBubble());
  EXPECT_FALSE(
      controller_->GetPartialTranslateBubble()->GetWidget()->IsVisible());

  fake_partial_translate_bubble_model_->NotifyTranslated();
  EXPECT_TRUE(
      controller_->GetPartialTranslateBubble()->GetWidget()->IsVisible());
  EXPECT_EQ(fake_partial_translate_bubble_model_->GetViewState(),
            PartialTranslateBubbleModel::ViewState::VIEW_STATE_AFTER_TRANSLATE);
}

TEST_F(TranslateBubbleControllerTest, PartialTranslateError) {
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  fake_partial_translate_bubble_model_->SetError(
      translate::TranslateErrors::TRANSLATION_ERROR);
  fake_partial_translate_bubble_model_->NotifyTranslated();
  EXPECT_TRUE(
      controller_->GetPartialTranslateBubble()->GetWidget()->IsVisible());
  EXPECT_EQ(fake_partial_translate_bubble_model_->GetViewState(),
            PartialTranslateBubbleModel::ViewState::VIEW_STATE_ERROR);
}

TEST_F(TranslateBubbleControllerTest, PartialTranslateSourceTextTruncatedTrue) {
  // Generate a string strictly larger than the text selection character limit.
  std::u16string string_to_truncate(
      translate::kDesktopPartialTranslateTextSelectionMaxCharacters + 1, '*');
  // Check that source_text_truncated_ is properly set for a new bubble.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", string_to_truncate);
  // Wait for bubble creation to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      fake_partial_translate_bubble_model_->GetSourceTextTruncatedForTest());

  // Check that source_text_truncated_ is properly set for an existing bubble.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", string_to_truncate);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      fake_partial_translate_bubble_model_->GetSourceTextTruncatedForTest());
}

TEST_F(TranslateBubbleControllerTest,
       PartialTranslateSourceTextTruncatedFalse) {
  // Check that source_text_truncated_ is properly set for a new bubble for a
  // string under the text selection character limit.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  // Wait for bubble creation to complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      fake_partial_translate_bubble_model_->GetSourceTextTruncatedForTest());

  // Check that source_text_truncated_ is properly set for an existing bubble.
  controller_->StartPartialTranslate(anchor_widget_->GetContentsView(), nullptr,
                                     "fr", "en", std::u16string());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      fake_partial_translate_bubble_model_->GetSourceTextTruncatedForTest());
}
