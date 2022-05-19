// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/translate/core/browser/mock_translate_metrics_logger.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
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

  void ShowError(translate::TranslateErrors::Type error_type) override {}

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

  bool ShouldNeverTranslateLanguage() override {
    return never_translate_language_;
  }

  void SetNeverTranslateLanguage(bool value) override {}

  bool ShouldNeverTranslateSite() override { return never_translate_site_; }

  void SetNeverTranslateSite(bool value) override {}

  bool ShouldAlwaysTranslateBeCheckedByDefault() const override {
    return false;
  }

  bool ShouldShowAlwaysTranslateShortcut() const override { return false; }

  bool ShouldAlwaysTranslate() const override {
    return should_always_translate_;
  }

  void SetAlwaysTranslate(bool value) override {}

  void Translate() override {}

  void RevertTranslation() override {}

  void OnBubbleClosing() override {}

  bool IsPageTranslatedInCurrentLanguages() const override { return false; }

  bool CanAddSiteToNeverPromptList() override { return false; }

  void ReportUIInteraction(translate::UIInteraction ui_interaction) override {}

  void ReportUIChange(bool is_ui_shown) override {}

  ViewState current_view_state_;
  bool never_translate_language_ = false;
  bool never_translate_site_ = false;
  bool should_always_translate_ = false;
};

class FakePartialTranslateBubbleModel : public PartialTranslateBubbleModel {
 public:
  explicit FakePartialTranslateBubbleModel(
      PartialTranslateBubbleModel::ViewState view_state) {
    DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
    DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
    current_view_state_ = view_state;
  }

  PartialTranslateBubbleModel::ViewState GetViewState() const override {
    return current_view_state_;
  }

  void SetViewState(
      PartialTranslateBubbleModel::ViewState view_state) override {
    current_view_state_ = view_state;
  }

  void ShowError(translate::TranslateErrors::Type error_type) override {}

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

  void Translate() override {}

  void RevertTranslation() override {}

  bool IsCurrentSelectionTranslated() const override { return false; }

  ViewState current_view_state_;
};

}  // namespace

class TranslateBubbleControllerTest : public ChromeViewsTestBase {
 public:
  TranslateBubbleControllerTest() = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // Owned by WebContents.
    controller_ = new TranslateBubbleController(web_contents_.get());
    web_contents_->SetUserData(TranslateBubbleController::UserDataKey(),
                               base::WrapUnique(controller_.get()));

    // Use fake translate bubble models instead of real implementations for
    // translate bubble view construction in tests.
    controller_->SetTranslateBubbleModelFactory(base::BindRepeating(
        &TranslateBubbleControllerTest::GetFakeTranslateBubbleModel,
        base::Unretained(this)));
    controller_->SetPartialTranslateBubbleModelFactory(base::BindRepeating(
        &TranslateBubbleControllerTest::GetFakePartialTranslateBubbleModel,
        base::Unretained(this)));
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<TranslateBubbleModel> GetFakeTranslateBubbleModel() {
    return std::make_unique<FakeTranslateBubbleModel>(
        TranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE);
  }

  std::unique_ptr<PartialTranslateBubbleModel>
  GetFakePartialTranslateBubbleModel() {
    return std::make_unique<FakePartialTranslateBubbleModel>(
        PartialTranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE);
  }

  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<translate::testing::MockTranslateMetricsLogger>
      mock_translate_metrics_logger_;

  // Owned by WebContents.
  raw_ptr<TranslateBubbleController> controller_;
};

TEST_F(TranslateBubbleControllerTest, ShowFullPageThenPartialTranslateBubble) {
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());

  // Show the full page translate bubble first.
  controller_->ShowTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
      translate::TranslateErrors::Type::NONE,
      LocationBarBubbleDelegateView::DisplayReason::AUTOMATIC);

  EXPECT_THAT(controller_->GetTranslateBubble(), testing::NotNull());

  // Showing the partial translate bubble while the full page translate bubble
  // is open should close the full translate bubble.
  controller_->ShowPartialTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      PartialTranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE, "fr",
      "en", translate::TranslateErrors::Type::NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::NotNull());
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());

  // Only the partial translate bubble should remain, close it.
  controller_->CloseBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());
}

TEST_F(TranslateBubbleControllerTest, ShowPartialThenFullPageTranslateBubble) {
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());

  // Show the partial translate bubble first.
  controller_->ShowPartialTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      PartialTranslateBubbleModel::ViewState::VIEW_STATE_BEFORE_TRANSLATE, "fr",
      "en", translate::TranslateErrors::Type::NONE);
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::NotNull());

  // Showing the full page translate bubble while the partial translate bubble
  // is open should close the partial translate bubble.
  controller_->ShowTranslateBubble(
      anchor_widget_->GetContentsView(), nullptr,
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE, "fr", "en",
      translate::TranslateErrors::Type::NONE,
      LocationBarBubbleDelegateView::DisplayReason::AUTOMATIC);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::NotNull());
  EXPECT_THAT(controller_->GetPartialTranslateBubble(), testing::IsNull());

  // Only the full page translate bubble should remain, close it.
  controller_->CloseBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(controller_->GetTranslateBubble(), testing::IsNull());
}
