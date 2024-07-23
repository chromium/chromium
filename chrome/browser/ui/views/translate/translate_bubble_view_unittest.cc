// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace {

class MockTranslateBubbleModel : public TranslateBubbleModel {
 public:
  explicit MockTranslateBubbleModel(TranslateBubbleModel::ViewState view_state)
      : error_type_(translate::TranslateErrors::NONE),
        source_language_index_(1),
        target_language_index_(2),
        never_translate_language_(false),
        never_translate_site_(false),
        should_show_always_translate_sortcut_(false),
        should_always_translate_(false),
        always_translate_checked_(false),
        set_always_translate_called_count_(0),
        translate_called_(false),
        revert_translation_called_(false),
        translation_declined_(false),
        source_language_index_on_translation_(-1),
        target_language_index_on_translation_(-1),
        can_add_site_to_never_prompt_list(true) {
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

  void ShowError(translate::TranslateErrors error_type) override {
    error_type_ = error_type;
  }

  int GetNumberOfSourceLanguages() const override { return 1000; }

  int GetNumberOfTargetLanguages() const override { return 1000; }

  std::u16string GetSourceLanguageNameAt(int index) const override {
    return u"English";
  }

  std::u16string GetTargetLanguageNameAt(int index) const override {
    return u"English";
  }

  std::string GetSourceLanguageCode() const override {
    if (source_language_index_ == 0)
      return "und";
    return "eng-US";
  }

  int GetSourceLanguageIndex() const override { return source_language_index_; }

  void UpdateSourceLanguageIndex(int index) override {
    source_language_index_ = index;
  }

  int GetTargetLanguageIndex() const override { return target_language_index_; }

  void UpdateTargetLanguageIndex(int index) override {
    target_language_index_ = index;
  }

  void DeclineTranslation() override { translation_declined_ = true; }

  bool ShouldNeverTranslateLanguage() override {
    return never_translate_language_;
  }

  void SetNeverTranslateLanguage(bool value) override {
    never_translate_language_ = value;
  }

  bool ShouldNeverTranslateSite() override { return never_translate_site_; }

  void SetNeverTranslateSite(bool value) override {
    never_translate_site_ = value;
  }

  bool ShouldAlwaysTranslateBeCheckedByDefault() const override {
    return always_translate_checked_;
  }

  bool ShouldShowAlwaysTranslateShortcut() const override {
    return should_show_always_translate_sortcut_;
  }

  void SetShouldShowAlwaysTranslateShortcut(bool value) {
    should_show_always_translate_sortcut_ = value;
  }

  bool ShouldAlwaysTranslate() const override {
    return should_always_translate_;
  }

  void SetAlwaysTranslate(bool value) override {
    should_always_translate_ = value;
    set_always_translate_called_count_++;
  }

  void Translate() override {
    translate_called_ = true;
    source_language_index_on_translation_ = source_language_index_;
    target_language_index_on_translation_ = target_language_index_;
  }

  void RevertTranslation() override { revert_translation_called_ = true; }

  void OnBubbleClosing() override {}

  bool IsPageTranslatedInCurrentLanguages() const override {
    return source_language_index_on_translation_ == source_language_index_ &&
           target_language_index_on_translation_ == target_language_index_;
  }

  bool CanAddSiteToNeverPromptList() override {
    return can_add_site_to_never_prompt_list;
  }

  void SetCanAddSiteToNeverPromptList(bool value) {
    can_add_site_to_never_prompt_list = value;
  }

  void ReportUIInteraction(translate::UIInteraction ui_interaction) override {}

  void ReportUIChange(bool is_ui_shown) override {}

  ViewState current_view_state_;
  translate::TranslateErrors error_type_;
  int source_language_index_;
  int target_language_index_;
  bool never_translate_language_;
  bool never_translate_site_;
  bool should_show_always_translate_sortcut_;
  bool should_always_translate_;
  bool always_translate_checked_;
  int set_always_translate_called_count_;
  bool translate_called_;
  bool revert_translation_called_;
  bool translation_declined_;
  int source_language_index_on_translation_;
  int target_language_index_on_translation_;
  bool can_add_site_to_never_prompt_list;
};

}  // namespace

class TranslateBubbleViewTest : public ChromeViewsTestBase {
 public:
  TranslateBubbleViewTest() = default;

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // The bubble needs the parent as an anchor.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Show();

    mock_model_ = new MockTranslateBubbleModel(
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  }

  void CreateAndShowBubble() {
    std::unique_ptr<TranslateBubbleModel> model(mock_model_);
    bubble_ = new TranslateBubbleView(
        anchor_widget_->GetContentsView(), std::move(model),
        translate::TranslateErrors::NONE, nullptr, base::DoNothing());
    views::BubbleDialogDelegateView::CreateBubble(bubble_)->Show();
  }

  void PressButton(TranslateBubbleView::ButtonID id) {
    views::Button* button =
        static_cast<views::Button*>(bubble_->GetViewByID(id));
    views::test::ButtonTestApi(button).NotifyClick(
        ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                     ui::DomCode::ENTER, ui::EF_NONE));
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  bool denial_button_clicked() { return mock_model_->translation_declined_; }

  void TriggerOptionsMenu() {
    PressButton(TranslateBubbleView::BUTTON_ID_OPTIONS_MENU);
  }

  ui::SimpleMenuModel* options_menu_model() {
    return bubble_->options_menu_model_.get();
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<MockTranslateBubbleModel, DanglingUntriaged> mock_model_;
  raw_ptr<TranslateBubbleView, DanglingUntriaged> bubble_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TranslateBubbleViewTest, TargetLanguageTabTriggersTranslate) {
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the target language tab to start translation.
  bubble_->TabSelectedAt(1);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, OptionsMenuNeverTranslateLanguage) {
  CreateAndShowBubble();

  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(mock_model_->never_translate_language_);
  EXPECT_FALSE(denial_button_clicked());
  TriggerOptionsMenu();

  const size_t index =
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE)
          .value();
  bubble_->options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_language_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, OptionsMenuNeverTranslateSite) {
  // NEVER_TRANSLATE_SITE should only show up for sites that can be blocklisted.
  mock_model_->SetCanAddSiteToNeverPromptList(true);
  CreateAndShowBubble();

  EXPECT_FALSE(mock_model_->never_translate_site_);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  TriggerOptionsMenu();
  const size_t index =
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::NEVER_TRANSLATE_SITE)
          .value();
  bubble_->options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_site_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxShortcut) {
  mock_model_->SetShouldShowAlwaysTranslateShortcut(true);
  CreateAndShowBubble();

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should affect the model right away.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->always_translate_checkbox_->GetChecked());

  // Click the checkbox. The state is saved.
  PressButton(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_TRUE(mock_model_->should_always_translate_);
  EXPECT_EQ(1, mock_model_->set_always_translate_called_count_);
  EXPECT_TRUE(bubble_->always_translate_checkbox_->GetChecked());
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble_->GetViewState());
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(), size_t{1});
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndCloseButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should NOT affect the model after pressing the cancel button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->advanced_always_translate_checkbox_->GetChecked());

  // Click the checkbox. The state is not saved yet.
  bubble_->advanced_always_translate_checkbox_->SetChecked(true);
  PressButton(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the cancel button. The state is not saved.
  PressButton(TranslateBubbleView::BUTTON_ID_CLOSE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndDoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should affect the model after pressing the done button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->advanced_always_translate_checkbox_->GetChecked());

  // Click the checkbox. The state is not saved yet.
  PressButton(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the done button. The state is saved.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->should_always_translate_);
  EXPECT_EQ(1, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, SourceResetButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // If there is no change in language selection, the reset button should be
  // disabled.
  EXPECT_FALSE(bubble_->advanced_reset_button_source_->GetEnabled());

  // Change the language selection. The reset button should be enabled.
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->SourceLanguageChanged();
  EXPECT_EQ(10u, bubble_->source_language_combobox_->GetSelectedIndex());
  EXPECT_TRUE(bubble_->advanced_reset_button_source_->GetEnabled());

  // Press the reset button. Language should change back to initial selection.
  PressButton(TranslateBubbleView::BUTTON_ID_RESET);
  EXPECT_EQ(1u, bubble_->source_language_combobox_->GetSelectedIndex());
  EXPECT_FALSE(bubble_->advanced_reset_button_source_->GetEnabled());
}

TEST_F(TranslateBubbleViewTest, TargetResetButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  // If there is no change in language selection, the reset button should be
  // disabled.
  EXPECT_FALSE(bubble_->advanced_reset_button_target_->GetEnabled());

  // Change the language selection. The reset button should be enabled.
  bubble_->target_language_combobox_->SetSelectedIndex(10);
  bubble_->TargetLanguageChanged();
  EXPECT_EQ(10u, bubble_->target_language_combobox_->GetSelectedIndex());
  EXPECT_TRUE(bubble_->advanced_reset_button_target_->GetEnabled());

  // Press the reset button. Language should change back to initial selection.
  PressButton(TranslateBubbleView::BUTTON_ID_RESET);
  EXPECT_EQ(2u, bubble_->target_language_combobox_->GetSelectedIndex());
}

TEST_F(TranslateBubbleViewTest, SourceDoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->SourceLanguageChanged();
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->TargetLanguageChanged();
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(10, mock_model_->source_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, TargetDoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->SourceLanguageChanged();
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->TargetLanguageChanged();
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(10, mock_model_->source_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, DoneButtonWithoutTranslating) {
  CreateAndShowBubble();

  // Translate the page once.
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);

  // Set translation called to false so it can be verified that translation is
  // not called again.
  mock_model_->translate_called_ = false;

  // Click the "Done" button with the current language pair. This time,
  // translation is not performed and the view state will stay at the translated
  // view.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_FALSE(mock_model_->translate_called_);

  // The page is already in the translated languages if Done doesn't trigger a
  // translation. Clicking done ensures that the UI is in the after translation
  // state.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, OptionsMenuRespectsBlocklistSite) {
  mock_model_->SetCanAddSiteToNeverPromptList(false);
  CreateAndShowBubble();

  TriggerOptionsMenu();
  // NEVER_TRANSLATE_SITE shouldn't show up for sites that can't be blocklisted.
  EXPECT_FALSE(
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::NEVER_TRANSLATE_SITE)
          .has_value());
  // Verify that the menu is populated so previous check makes sense.
  EXPECT_TRUE(
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE)
          .has_value());
}

TEST_F(TranslateBubbleViewTest, MenuOptionsHiddenOnUnknownSource) {
  // Set source language to "Unknown".
  mock_model_->UpdateSourceLanguageIndex(0);
  CreateAndShowBubble();

  TriggerOptionsMenu();
  // NEVER_TRANSLATE_LANGUAGE and ALWAYS_TRANSLATE_LANGUAGE shouldn't show when
  // the source language is "Unknown".
  EXPECT_FALSE(
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE)
          .has_value());
  EXPECT_FALSE(
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE)
          .has_value());
  // Verify that the menu is populated so previous checks make sense.
  EXPECT_TRUE(
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::CHANGE_TARGET_LANGUAGE)
          .has_value());
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateLanguageMenuItem) {
  CreateAndShowBubble();

  TriggerOptionsMenu();
  const size_t index =
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE)
          .value();

  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(bubble_->options_menu_model_->IsItemCheckedAt(index));
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);

  // Toggle ShouldAlwaysTranslate and make sure Translation doesn't happen
  // again.
  mock_model_->translate_called_ = false;
  // Revert Always Translate.
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_FALSE(mock_model_->translate_called_);
  // Recheck Always Translate.
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_FALSE(mock_model_->translate_called_);

  // Go back to untranslated page, since the *language* should still always
  // be translated (and this "untranslate" is temporary) the option should now
  // be checked and it should be possible to disable it from the menu.
  bubble_->TabSelectedAt(0);
  EXPECT_TRUE(mock_model_->revert_translation_called_);

  TriggerOptionsMenu();
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(bubble_->options_menu_model_->IsItemCheckedAt(index));

  // Translate should not be called when disabling always-translate. The page is
  // not currently in a translated state and nothing needs to be reverted.
  // translate_called_ is set back to false just to make sure it's not being
  // called again.
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(mock_model_->translate_called_);

  TriggerOptionsMenu();
  EXPECT_FALSE(bubble_->options_menu_model_->IsItemCheckedAt(index));
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateTriggerTranslation) {
  CreateAndShowBubble();

  TriggerOptionsMenu();
  const size_t index =
      bubble_->options_menu_model_
          ->GetIndexOfCommandId(TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE)
          .value();

  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(bubble_->options_menu_model_->IsItemCheckedAt(index));
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            mock_model_->GetViewState());

  // De-select Always Translate while the page stays translated and in state
  // VIEW_STATE_TRANSLATING
  mock_model_->SetAlwaysTranslate(false);
  mock_model_->RevertTranslation();
  EXPECT_TRUE(mock_model_->revert_translation_called_);

  // Select Always Translate Again should trigger translation
  bubble_->options_menu_model_->ActivatedAt(index);
  mock_model_->SetAlwaysTranslate(true);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_TRUE(mock_model_->IsPageTranslatedInCurrentLanguages());
}

TEST_F(TranslateBubbleViewTest, TabSelectedAfterTranslation) {
  CreateAndShowBubble();
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(0));
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(1));
}

TEST_F(TranslateBubbleViewTest, SourceLanguageTabUpdatesViewState) {
  CreateAndShowBubble();
  // Select target language tab to translate.
  bubble_->TabSelectedAt(1);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_TRANSLATING,
            bubble_->GetViewState());

  // Select source language tab to revert translation.
  bubble_->TabSelectedAt(0);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}
