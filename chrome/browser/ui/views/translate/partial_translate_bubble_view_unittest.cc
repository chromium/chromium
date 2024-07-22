// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_ui_action_logger.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace {

class FakePartialTranslateBubbleModel : public PartialTranslateBubbleModel {
 public:
  explicit FakePartialTranslateBubbleModel(
      PartialTranslateBubbleModel::ViewState view_state) {
    DCHECK_NE(VIEW_STATE_SOURCE_LANGUAGE, view_state);
    DCHECK_NE(VIEW_STATE_TARGET_LANGUAGE, view_state);
    current_view_state_ = view_state;
  }

  void AddObserver(PartialTranslateBubbleModel::Observer* observer) override {}
  void RemoveObserver(
      PartialTranslateBubbleModel::Observer* observer) override {}

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

  void SetError(translate::TranslateErrors error_type) override {}
  translate::TranslateErrors GetError() const override {
    return translate::TranslateErrors::NONE;
  }

  int GetNumberOfSourceLanguages() const override { return 1000; }

  int GetNumberOfTargetLanguages() const override { return 1000; }

  std::u16string GetSourceLanguageNameAt(int index) const override {
    return source_name_;
  }

  std::u16string GetTargetLanguageNameAt(int index) const override {
    return target_name_;
  }

  int GetSourceLanguageIndex() const override { return 1; }

  void UpdateSourceLanguageIndex(int index) override {}

  int GetTargetLanguageIndex() const override { return 2; }

  void UpdateTargetLanguageIndex(int index) override {}

  std::string GetSourceLanguageCode() const override { return "en"; }

  std::string GetTargetLanguageCode() const override { return "en"; }

  void Translate(content::WebContents* web_contents) override {
    translate_called_ = true;
  }

  void TranslateFullPage(content::WebContents* web_contents) override {
    full_page_translate_called_ = true;
  }

  void SetSourceTextTruncated(bool is_truncated) override {}

  ViewState current_view_state_;
  std::u16string source_name_ = u"English";
  std::u16string target_name_ = u"English";
  bool translate_called_ = false;
  bool full_page_translate_called_ = false;
};

}  // namespace

class PartialTranslateBubbleViewTest : public ChromeViewsTestBase {
 public:
  PartialTranslateBubbleViewTest() = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // The bubble needs the parent as an anchor.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();

    mock_model_ = new FakePartialTranslateBubbleModel(
        PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  }

  void CreateAndShowBubble() {
    std::unique_ptr<PartialTranslateBubbleModel> model(mock_model_);
    bubble_ = new PartialTranslateBubbleView(anchor_widget_->GetContentsView(),
                                             std::move(model), nullptr,
                                             base::DoNothing());
    views::BubbleDialogDelegateView::CreateBubble(bubble_)->Show();
  }

  void PressButton(PartialTranslateBubbleView::ButtonID id) {
    views::Button* button =
        static_cast<views::Button*>(bubble_->GetViewByID(id));
    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::DomCode::ENTER, ui::EF_NONE);
    views::test::ButtonTestApi(button).NotifyClick(key_event);
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<FakePartialTranslateBubbleModel, DanglingUntriaged> mock_model_;
  raw_ptr<PartialTranslateBubbleView, DanglingUntriaged> bubble_;
};

TEST_F(PartialTranslateBubbleViewTest,
       TargetLanguageTabDoesntTriggerTranslate) {
  base::HistogramTester histogram_tester;
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the target language tab to start translation.
  bubble_->TabSelectedAt(1);
  EXPECT_FALSE(mock_model_->translate_called_);
  histogram_tester.ExpectUniqueSample(
      translate::kPartialTranslateBubbleUiEventHistogramName,
      translate::PartialTranslateBubbleUiEvent::TARGET_LANGUAGE_TAB_SELECTED,
      1);
}

TEST_F(PartialTranslateBubbleViewTest, TabSelectedAfterTranslation) {
  CreateAndShowBubble();
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(0));
  bubble_->SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(1));
}

TEST_F(PartialTranslateBubbleViewTest, UpdateLanguageTabsFromResponse) {
  CreateAndShowBubble();
  EXPECT_EQ(bubble_->tabbed_pane_->GetTabAt(0)->GetTitleText(), u"English");
  EXPECT_EQ(bubble_->tabbed_pane_->GetTabAt(1)->GetTitleText(), u"English");

  mock_model_->source_name_ = u"Japanese";
  mock_model_->target_name_ = u"French";
  bubble_->SwitchView(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);

  EXPECT_EQ(bubble_->tabbed_pane_->GetTabAt(0)->GetTitleText(), u"Japanese");
  EXPECT_EQ(bubble_->tabbed_pane_->GetTabAt(1)->GetTitleText(), u"French");
}

TEST_F(PartialTranslateBubbleViewTest, SourceLanguageTabUpdatesViewState) {
  CreateAndShowBubble();
  // Select target language tab to translate.
  bubble_->TabSelectedAt(1);
  EXPECT_EQ(PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());

  // Select source language tab to revert translation.
  bubble_->TabSelectedAt(0);
  EXPECT_EQ(PartialTranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

// TODO(crbug.com/40848161): For some reason calling bubble_->TabSelectedAt(1)
// before bubble_->TabSelectedAt(0) in a test causes TabSelectedAt(0) to be
// run twice, resulting in the corresponding sample being logged twice. This
// does not happen in production. For now, test this logging separately to avoid
// encountering this issue in TabSelectedAfterTranslation.
TEST_F(PartialTranslateBubbleViewTest, SourceLanguageTabSelectedLogged) {
  base::HistogramTester histogram_tester;
  CreateAndShowBubble();
  bubble_->TabSelectedAt(0);
  histogram_tester.ExpectBucketCount(
      translate::kPartialTranslateBubbleUiEventHistogramName,
      translate::PartialTranslateBubbleUiEvent::SOURCE_LANGUAGE_TAB_SELECTED,
      1);
}

TEST_F(PartialTranslateBubbleViewTest, TranslateFullPageButton) {
  CreateAndShowBubble();
  PressButton(PartialTranslateBubbleView::BUTTON_ID_FULL_PAGE_TRANSLATE);
  EXPECT_TRUE(mock_model_->full_page_translate_called_);
}
