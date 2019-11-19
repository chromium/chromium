// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"

namespace {

class MockTranslateBubbleModel : public TranslateBubbleModel {
 public:
  explicit MockTranslateBubbleModel(TranslateBubbleModel::ViewState view_state)
      : view_state_transition_(view_state),
        error_type_(translate::TranslateErrors::NONE),
        original_language_index_(0),
        target_language_index_(1),
        never_translate_language_(false),
        never_translate_site_(false),
        should_always_translate_(false),
        always_translate_checked_(false),
        set_always_translate_called_count_(0),
        translate_called_(false),
        revert_translation_called_(false),
        translation_declined_(false),
        original_language_index_on_translation_(-1),
        target_language_index_on_translation_(-1),
        can_blacklist_site_(true) {}

  TranslateBubbleModel::ViewState GetViewState() const override {
    return view_state_transition_.view_state();
  }

  void SetViewState(TranslateBubbleModel::ViewState view_state) override {
    view_state_transition_.SetViewState(view_state);
  }

  void ShowError(translate::TranslateErrors::Type error_type) override {
    error_type_ = error_type;
  }

  void GoBackFromAdvanced() override {
    view_state_transition_.GoBackFromAdvanced();
  }

  int GetNumberOfLanguages() const override { return 1000; }

  base::string16 GetLanguageNameAt(int index) const override {
    return base::ASCIIToUTF16("English");
  }

  int GetOriginalLanguageIndex() const override {
    return original_language_index_;
  }

  void UpdateOriginalLanguageIndex(int index) override {
    original_language_index_ = index;
  }

  int GetTargetLanguageIndex() const override { return target_language_index_; }

  void UpdateTargetLanguageIndex(int index) override {
    target_language_index_ = index;
  }

  void DeclineTranslation() override { translation_declined_ = true; }

  void SetNeverTranslateLanguage(bool value) override {
    never_translate_language_ = value;
  }

  void SetNeverTranslateSite(bool value) override {
    never_translate_site_ = value;
  }

  bool ShouldAlwaysTranslateBeCheckedByDefault() const override {
    return always_translate_checked_;
  }

  bool ShouldAlwaysTranslate() const override {
    return should_always_translate_;
  }

  bool ShouldShowAlwaysTranslateShortcut() const override { return false; }

  void SetAlwaysTranslate(bool value) override {
    should_always_translate_ = value;
    set_always_translate_called_count_++;
  }

  void Translate() override {
    translate_called_ = true;
    original_language_index_on_translation_ = original_language_index_;
    target_language_index_on_translation_ = target_language_index_;
  }

  void RevertTranslation() override { revert_translation_called_ = true; }

  void OnBubbleClosing() override {}

  bool IsPageTranslatedInCurrentLanguages() const override {
    return original_language_index_on_translation_ ==
               original_language_index_ &&
           target_language_index_on_translation_ == target_language_index_;
  }

  bool CanBlacklistSite() override { return can_blacklist_site_; }

  void SetCanBlacklistSite(bool value) { can_blacklist_site_ = value; }

  TranslateBubbleViewStateTransition view_state_transition_;
  translate::TranslateErrors::Type error_type_;
  int original_language_index_;
  int target_language_index_;
  bool never_translate_language_;
  bool never_translate_site_;
  bool should_always_translate_;
  bool always_translate_checked_;
  int set_always_translate_called_count_;
  bool translate_called_;
  bool revert_translation_called_;
  bool translation_declined_;
  int original_language_index_on_translation_;
  int target_language_index_on_translation_;
  bool can_blacklist_site_;
};

}  // namespace

class TranslateBubbleViewTest : public ChromeViewsTestBase {
 public:
  TranslateBubbleViewTest() {}

 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // The bubble needs the parent as an anchor.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();

    mock_model_ = new MockTranslateBubbleModel(
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  }

  void CreateAndShowBubble() {
    std::unique_ptr<TranslateBubbleModel> model(mock_model_);
    bubble_ = new TranslateBubbleView(anchor_widget_->GetContentsView(),
                                      std::move(model),
                                      translate::TranslateErrors::NONE, NULL);
    views::BubbleDialogDelegateView::CreateBubble(bubble_)->Show();
  }

  void PressButton(TranslateBubbleView::ButtonID id) {
    views::LabelButton button(nullptr, base::ASCIIToUTF16("hello"));
    button.SetID(id);

    bubble_->ButtonPressed(&button,
                           ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                        ui::DomCode::ENTER, ui::EF_NONE));
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

  bool denial_button_clicked() { return mock_model_->translation_declined_; }
  void TriggerOptionsMenu() {
    bubble_->ButtonPressed(bubble_->before_translate_options_button_,
                           ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                        ui::DomCode::ENTER, ui::EF_NONE));
  }

  void TriggerOptionsMenuTab() {
    views::Button* button = static_cast<views::Button*>(
        bubble_->GetViewByID(TranslateBubbleView::BUTTON_ID_OPTIONS_MENU_TAB));
    LOG(INFO) << button->GetID();
    bubble_->ButtonPressed(button,
                           ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                                        ui::DomCode::ENTER, ui::EF_NONE));
  }

  ui::SimpleMenuModel* options_menu_model() {
    return bubble_->options_menu_model_.get();
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  MockTranslateBubbleModel* mock_model_;
  TranslateBubbleView* bubble_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TranslateBubbleViewTest, TranslateButton) {
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the "Translate" button.
  PressButton(TranslateBubbleView::BUTTON_ID_TRANSLATE);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, TabUiTranslateButton) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the "Translate" button.
  PressButton(TranslateBubbleView::BUTTON_ID_TRANSLATE);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, OptionsMenuNeverTranslateLanguage) {
  CreateAndShowBubble();

  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(mock_model_->never_translate_language_);
  EXPECT_FALSE(denial_button_clicked());

  TriggerOptionsMenu();
  const int index = bubble_->options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE);
  bubble_->options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_language_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, TabUiOptionsMenuNeverTranslateLanguage) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();

  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(mock_model_->never_translate_language_);
  EXPECT_FALSE(denial_button_clicked());
  TriggerOptionsMenuTab();

  const int index = bubble_->tab_options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE);
  bubble_->tab_options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_language_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, OptionsMenuNeverTranslateSite) {
  // NEVER_TRANSLATE_SITE should only show up for sites that can be blacklisted.
  mock_model_->SetCanBlacklistSite(true);
  CreateAndShowBubble();

  EXPECT_FALSE(mock_model_->never_translate_site_);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  TriggerOptionsMenu();
  const int index = bubble_->options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::NEVER_TRANSLATE_SITE);
  bubble_->options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_site_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, TabUiOptionsMenuNeverTranslateSite) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  // NEVER_TRANSLATE_SITE should only show up for sites that can be blacklisted.
  mock_model_->SetCanBlacklistSite(true);
  CreateAndShowBubble();

  EXPECT_FALSE(mock_model_->never_translate_site_);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  TriggerOptionsMenuTab();
  const int index = bubble_->tab_options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::NEVER_TRANSLATE_SITE);
  bubble_->tab_options_menu_model_->ActivatedAt(index);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_site_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

// This button is not used in Tab Ui.
TEST_F(TranslateBubbleViewTest, ShowOriginalButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);

  // Click the "Show original" button to revert translation.
  EXPECT_FALSE(mock_model_->revert_translation_called_);
  PressButton(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);
  EXPECT_TRUE(mock_model_->revert_translation_called_);
}

// This button is not used in Tab Ui.
TEST_F(TranslateBubbleViewTest, TryAgainButton) {
  CreateAndShowBubble();
  bubble_->SwitchToErrorView(translate::TranslateErrors::NETWORK);

  EXPECT_EQ(translate::TranslateErrors::NETWORK, mock_model_->error_type_);

  // Click the "Try again" button to translate.
  EXPECT_FALSE(mock_model_->translate_called_);
  PressButton(TranslateBubbleView::BUTTON_ID_TRY_AGAIN);
  EXPECT_TRUE(mock_model_->translate_called_);
}

// This checkbox is not used in Tab Ui.
TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndCancelButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

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
  PressButton(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
}

// This checkbox is not used in Tab Ui.
TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndDoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should affect the model after pressing the done button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->advanced_always_translate_checkbox_->GetChecked());

  // Click the checkbox. The state is not saved yet.
  bubble_->advanced_always_translate_checkbox_->SetChecked(true);
  PressButton(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the done button. The state is saved.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->should_always_translate_);
  EXPECT_EQ(1, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, DoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_SOURCE_LANGUAGE);
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_TARGET_LANGUAGE);
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  // Expected value is (set id - 1) because user selected id is actual id + 1
  EXPECT_EQ(9, mock_model_->original_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);
}

TEST_F(TranslateBubbleViewTest, TabUiSourceDoneButton) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_SOURCE_LANGUAGE);
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_TARGET_LANGUAGE);
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  // Expected value is (set id - 1) because user selected id is actual id + 1
  EXPECT_EQ(9, mock_model_->original_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);
}

TEST_F(TranslateBubbleViewTest, TabUiTargetDoneButton) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_SOURCE_LANGUAGE);
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_TARGET_LANGUAGE);
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(9, mock_model_->original_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);
}

TEST_F(TranslateBubbleViewTest, DoneButtonWithoutTranslating) {
  CreateAndShowBubble();
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Translate the page once.
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to the initial view.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  mock_model_->translate_called_ = false;

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Done" button with the current language pair. This time,
  // translation is not performed and the view state will be back to the
  // previous view.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_FALSE(mock_model_->translate_called_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, TabUiSourceDoneButtonWithoutTranslating) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Translate the page once.
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to the initial view.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  mock_model_->translate_called_ = false;

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE);

  // Click the "Done" button with the current language pair. This time,
  // translation is not performed and the view state will be back to the
  // previous view.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_FALSE(mock_model_->translate_called_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, TabUiTargetDoneButtonWithoutTranslating) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Translate the page once.
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to the initial view.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  mock_model_->translate_called_ = false;

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);

  // Click the "Done" button with the current language pair. This time,
  // translation is not performed and the view state will be back to the
  // previous view.
  PressButton(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_FALSE(mock_model_->translate_called_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningBeforeTranslate) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  PressButton(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningAfterTranslate) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  PressButton(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningError) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  PressButton(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ERROR, bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, OptionsMenuRespectsBlacklistSite) {
  mock_model_->SetCanBlacklistSite(false);
  CreateAndShowBubble();

  TriggerOptionsMenu();
  // NEVER_TRANSLATE_SITE shouldn't show up for sites that can't be blacklisted.
  EXPECT_EQ(-1, bubble_->options_menu_model_->GetIndexOfCommandId(
                    TranslateBubbleView::NEVER_TRANSLATE_SITE));
  // Verify that the menu is populated so previous check makes sense.
  EXPECT_GE(bubble_->options_menu_model_->GetIndexOfCommandId(
                TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE),
            0);
}

TEST_F(TranslateBubbleViewTest, TabUiOptionsMenuRespectsBlacklistSite) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  mock_model_->SetCanBlacklistSite(false);
  CreateAndShowBubble();

  TriggerOptionsMenuTab();
  // NEVER_TRANSLATE_SITE shouldn't show up for sites that can't be blacklisted.
  EXPECT_EQ(-1, bubble_->tab_options_menu_model_->GetIndexOfCommandId(
                    TranslateBubbleView::NEVER_TRANSLATE_SITE));
  // Verify that the menu is populated so previous check makes sense.
  EXPECT_GE(bubble_->tab_options_menu_model_->GetIndexOfCommandId(
                TranslateBubbleView::NEVER_TRANSLATE_LANGUAGE),
            0);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateLanguageMenuItem) {
  CreateAndShowBubble();

  TriggerOptionsMenu();
  const int index = bubble_->options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE);

  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(bubble_->options_menu_model_->IsItemCheckedAt(index));
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to untranslated page, since the *language* should still always
  // be translated (and this "untranslate" is temporary) the option should now
  // be checked and it should be possible to disable it from the menu.
  PressButton(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);
  EXPECT_TRUE(mock_model_->revert_translation_called_);

  TriggerOptionsMenu();
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(bubble_->options_menu_model_->IsItemCheckedAt(index));

  // Translate should not be called when disabling always-translate. The page is
  // not currently in a translated state and nothing needs to be reverted.
  // translate_called_ is set back to false just to make sure it's not being
  // called again.
  mock_model_->translate_called_ = false;
  bubble_->options_menu_model_->ActivatedAt(index);
  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(mock_model_->translate_called_);

  TriggerOptionsMenu();
  EXPECT_FALSE(bubble_->options_menu_model_->IsItemCheckedAt(index));
}

TEST_F(TranslateBubbleViewTest, TabUiAlwaysTranslateLanguageMenuItem) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();

  TriggerOptionsMenuTab();
  const int index = bubble_->tab_options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE);

  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(bubble_->tab_options_menu_model_->IsItemCheckedAt(index));
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->tab_options_menu_model_->ActivatedAt(index);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to untranslated page, since the *language* should still always
  // be translated (and this "untranslate" is temporary) the option should now
  // be checked and it should be possible to disable it from the menu.
  PressButton(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);
  EXPECT_TRUE(mock_model_->revert_translation_called_);

  TriggerOptionsMenuTab();
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(bubble_->tab_options_menu_model_->IsItemCheckedAt(index));

  // Translate should not be called when disabling always-translate. The page is
  // not currently in a translated state and nothing needs to be reverted.
  // translate_called_ is set back to false just to make sure it's not being
  // called again.
  mock_model_->translate_called_ = false;
  bubble_->tab_options_menu_model_->ActivatedAt(index);
  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(mock_model_->translate_called_);

  TriggerOptionsMenuTab();
  EXPECT_FALSE(bubble_->tab_options_menu_model_->IsItemCheckedAt(index));
}

TEST_F(TranslateBubbleViewTest, TabUiAlwaysTranslateTriggerTranslation) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();

  TriggerOptionsMenuTab();
  const int index = bubble_->tab_options_menu_model_->GetIndexOfCommandId(
      TranslateBubbleView::ALWAYS_TRANSLATE_LANGUAGE);

  EXPECT_FALSE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_FALSE(bubble_->tab_options_menu_model_->IsItemCheckedAt(index));
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->tab_options_menu_model_->ActivatedAt(index);
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
  bubble_->tab_options_menu_model_->ActivatedAt(index);
  mock_model_->SetAlwaysTranslate(true);
  EXPECT_TRUE(mock_model_->ShouldAlwaysTranslate());
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_TRUE(mock_model_->IsPageTranslatedInCurrentLanguages());
}

TEST_F(TranslateBubbleViewTest, TabUiTabSelectedAfterTranslation) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      language::kUseButtonTranslateBubbleUi,
      {{language::kTranslateUIBubbleKey,
        language::kTranslateUIBubbleTabValue}});

  CreateAndShowBubble();
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(0));
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  EXPECT_EQ(bubble_->tabbed_pane_->GetSelectedTabIndex(),
            static_cast<size_t>(1));
}
