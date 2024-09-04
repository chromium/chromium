// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/source_language_combobox_model.h"
#include "chrome/browser/ui/translate/target_language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/non_client_view.h"

class Browser;

namespace translate {
class TranslateBubbleVisualTest;
}  // namespace translate

namespace views {
class Checkbox;
class Combobox;
class LabelButton;
class View;
}  // namespace views

class TranslateBubbleView : public LocationBarBubbleDelegateView,
                            public ui::SimpleMenuModel::Delegate,
                            public views::TabbedPaneListener {
  METADATA_HEADER(TranslateBubbleView, LocationBarBubbleDelegateView)

 public:
  // Item IDs for the option button's menu.
  enum OptionsMenuItem {
    ALWAYS_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_SITE,
    CHANGE_TARGET_LANGUAGE,
    CHANGE_SOURCE_LANGUAGE,
    OPEN_LANGUAGE_SETTINGS
  };

  // Element IDs for ui::ElementTracker.
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kIdentifier);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSourceLanguageTab);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTargetLanguageTab);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOptionsMenuButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChangeTargetLanguage);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTargetLanguageCombobox);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTargetLanguageDoneButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kChangeSourceLanguage);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSourceLanguageCombobox);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSourceLanguageDoneButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kErrorMessage);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOpenLanguageSettings);

  TranslateBubbleView(views::View* anchor_view,
                      std::unique_ptr<TranslateBubbleModel> model,
                      translate::TranslateErrors error_type,
                      content::WebContents* web_contents,
                      base::OnceClosure on_closing);

  TranslateBubbleView(const TranslateBubbleView&) = delete;
  TranslateBubbleView& operator=(const TranslateBubbleView&) = delete;

  ~TranslateBubbleView() override;

  void CloseTranslateBubble();

  TranslateBubbleModel* model() { return model_.get(); }

  // LocationBarBubbleDelegateView:
  void Init() override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnWidgetClosing(views::Widget* widget) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns the current view state.
  TranslateBubbleModel::ViewState GetViewState() const;

  // Initialize the bubble in the correct view state when it is shown.
  void SetViewState(translate::TranslateStep step,
                    translate::TranslateErrors error_type);

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;

 private:
  // IDs used by TranslateBubbleViewTest to simulate button presses.
  enum ButtonID {
    BUTTON_ID_DONE = 1,
    BUTTON_ID_TRY_AGAIN,
    BUTTON_ID_ALWAYS_TRANSLATE,
    BUTTON_ID_OPTIONS_MENU,
    BUTTON_ID_CLOSE,
    BUTTON_ID_RESET
  };

  friend class TranslateBubbleViewTest;
  friend class translate::TranslateBubbleVisualTest;
  friend void ::translate::test_utils::PressTranslate(::Browser*);
  friend void ::translate::test_utils::PressRevert(::Browser*);
  friend void ::translate::test_utils::SelectTargetLanguageByDisplayName(
      ::Browser*,
      const ::std::u16string&);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TargetLanguageTabTriggersTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxShortcut);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndCloseButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, SourceResetButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TargetResetButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, SourceDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TargetDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           DoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuRespectsBlocklistSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           MenuOptionsHiddenOnUnknownSource);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateLanguageMenuItem);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabSelectedAfterTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateTriggerTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateWithNeverTranslateSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           SourceLanguageTabUpdatesViewState);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // Returns the current child view.
  views::View* GetCurrentView() const;

  // Triggers options menu.
  void ShowOptionsMenu(views::Button* source);

  // Handles the event when the user changes an index of a combobox.
  void SourceLanguageChanged();
  void TargetLanguageChanged();

  void AlwaysTranslatePressed();

  // Updates the visibilities of child views according to the current view type.
  void UpdateChildVisibilities();

  // Creates the view used before/during/after translate.
  std::unique_ptr<views::View> CreateView();

  // AddTab function requires a view element to be shown below each tab.
  // This function creates an empty view so no extra white space below the tab.
  std::unique_ptr<views::View> CreateEmptyPane();

  // Creates the 'error' view for Button UI. Caller takes ownership of the
  // returned view.
  std::unique_ptr<views::View> CreateViewError();

  // Creates the 'error' view skeleton UI with no title. Caller takes ownership
  // of the returned view.
  std::unique_ptr<views::View> CreateViewErrorNoTitle(
      std::unique_ptr<views::Button> advanced_button);

  // Creates source language label and combobox for Tab UI advanced view. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvancedSource();

  // Creates target language label and combobox for Tab UI advanced view. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvancedTarget();

  // Creates the 'advanced' view to show source/target language combobox. Caller
  // takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvanced(
      std::unique_ptr<views::Combobox> combobox,
      std::unique_ptr<views::Label> language_title_label,
      std::unique_ptr<views::Button> advanced_reset_button,
      std::unique_ptr<views::Button> advanced_done_button,
      std::unique_ptr<views::Checkbox> advanced_always_translate_checkbox);

  // Creates a translate icon for when the bottom branding isn't showing. This
  // should only be used on non-Chrome-branded builds.
  std::unique_ptr<views::ImageView> CreateTranslateIcon();

  // Creates a three dot options menu button.
  std::unique_ptr<views::Button> CreateOptionsMenuButton();

  // Creates a close button.
  std::unique_ptr<views::Button> CreateCloseButton();

  // Get the current always translate checkbox.
  views::Checkbox* GetAlwaysTranslateCheckbox();

  // Sets the window title. The window title still needs to be set, even when it
  // is not shown, for accessibility purposes.
  void SetWindowTitle(TranslateBubbleModel::ViewState view_state);

  // Updates the view state. Whenever the view state is updated, the title needs
  // to be updated for accessibility.
  void UpdateViewState(TranslateBubbleModel::ViewState view_state);

  // Switches the view type.
  void SwitchView(TranslateBubbleModel::ViewState view_state);

  // Handles tab switching on when the view type switches.
  void SwitchTabForViewState(TranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  // Actions for button presses shared with accelerators.
  void Translate();
  void ShowOriginal();
  void ConfirmAdvancedOptions();

  // Returns whether or not the current language selection is different from the
  // initial language selection in an advanced view.
  bool DidLanguageSelectionChange(TranslateBubbleModel::ViewState view_state);

  // Handles the reset button in advanced view under Tab UI.
  void ResetLanguage();

  // Retrieve the names of the from/to languages and reset the language
  // indices.
  void UpdateLanguageNames(std::u16string* source_language_name,
                           std::u16string* target_language_name);

  void UpdateInsets(TranslateBubbleModel::ViewState state);

  // If the page is already translated, revert it. Otherwise decline
  // translation. Then close the bubble view.
  void RevertOrDeclineTranslation();

  // Helper method to announce the passed-in text to the screenreader.
  void AnnounceTextToScreenReader(const std::u16string& announcement_text);

  raw_ptr<views::View, DanglingUntriaged> translate_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> error_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> advanced_view_source_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> advanced_view_target_ = nullptr;

  raw_ptr<views::Combobox, DanglingUntriaged> source_language_combobox_ =
      nullptr;
  raw_ptr<views::Combobox, DanglingUntriaged> target_language_combobox_ =
      nullptr;

  raw_ptr<views::Checkbox, DanglingUntriaged> always_translate_checkbox_ =
      nullptr;
  raw_ptr<views::Checkbox, DanglingUntriaged>
      advanced_always_translate_checkbox_ = nullptr;
  raw_ptr<views::TabbedPane, DanglingUntriaged> tabbed_pane_ = nullptr;

  raw_ptr<views::LabelButton, DanglingUntriaged> advanced_reset_button_source_ =
      nullptr;
  raw_ptr<views::LabelButton, DanglingUntriaged> advanced_reset_button_target_ =
      nullptr;
  raw_ptr<views::LabelButton, DanglingUntriaged> advanced_done_button_source_ =
      nullptr;
  raw_ptr<views::LabelButton, DanglingUntriaged> advanced_done_button_target_ =
      nullptr;

  // Default source/target language without user interaction.
  size_t previous_source_language_index_;
  size_t previous_target_language_index_;

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  std::unique_ptr<TranslateBubbleModel> model_;

  translate::TranslateErrors error_type_;

  raw_ptr<actions::ActionItem> translate_action_item_ = nullptr;

  // Whether the window is an incognito window.
  const bool is_in_incognito_window_;

  bool should_always_translate_ = false;
  bool should_never_translate_language_ = false;
  bool should_never_translate_site_ = false;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  base::OnceClosure on_closing_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
