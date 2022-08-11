// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/translate/source_language_combobox_model.h"
#include "chrome/browser/ui/translate/target_language_combobox_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class Combobox;
class LabelButton;
class View;
}  // namespace views

class PartialTranslateBubbleView : public LocationBarBubbleDelegateView,
                                   public ui::SimpleMenuModel::Delegate,
                                   public views::TabbedPaneListener {
 public:
  // Item IDs for the option button's menu.
  enum OptionsMenuItem { CHANGE_TARGET_LANGUAGE, CHANGE_SOURCE_LANGUAGE };

  // Element IDs for ui::ElementTracker
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

  PartialTranslateBubbleView(views::View* anchor_view,
                             std::unique_ptr<PartialTranslateBubbleModel> model,
                             translate::TranslateErrors::Type error_type,
                             content::WebContents* web_contents,
                             const std::u16string& text_selection,
                             base::OnceClosure on_closing);

  PartialTranslateBubbleView(const PartialTranslateBubbleView&) = delete;
  PartialTranslateBubbleView& operator=(const PartialTranslateBubbleView&) =
      delete;

  ~PartialTranslateBubbleView() override;

  PartialTranslateBubbleModel* model() { return model_.get(); }

  // LocationBarBubbleDelegateView:
  void Init() override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns the current view state.
  PartialTranslateBubbleModel::ViewState GetViewState() const;

  // Initialize the bubble in the correct view state when it is shown.
  void SetViewState(PartialTranslateBubbleModel::ViewState view_state,
                    translate::TranslateErrors::Type error_type);

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;

 private:
  // IDs used by PartialTranslateBubbleViewTest to simulate button presses.
  enum ButtonID {
    BUTTON_ID_DONE = 1,
    BUTTON_ID_TRY_AGAIN,
    BUTTON_ID_OPTIONS_MENU,
    BUTTON_ID_CLOSE,
    BUTTON_ID_RESET,
    BUTTON_ID_FULL_PAGE_TRANSLATE
  };

  friend class PartialTranslateBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           TargetLanguageTabTriggersTranslate);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           TabSelectedAfterTranslation);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           SourceLanguageTabUpdatesViewState);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           SourceLanguageTabSelectedLogged);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           TranslateFullPageButton);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // Returns the current child view.
  views::View* GetCurrentView() const;

  // Triggers options menu.
  void ShowOptionsMenu(views::Button* source);

  // Handles the event when the user changes an index of a combobox.
  void SourceLanguageChanged();
  void TargetLanguageChanged();

  // Updates the visibilities of child views according to the current view type.
  void UpdateChildVisibilities();

  // Creates the view used before/during/after translate.
  std::unique_ptr<views::View> CreateView();

  // AddTab function requires a view element to be shown below each tab.
  // This function creates an empty view so no extra white space below the tab.
  std::unique_ptr<views::View> CreateEmptyPane();

  // Creates the 'error' view for Button UI.
  std::unique_ptr<views::View> CreateViewError();

  // Creates the 'error' view skeleton UI with no title.
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
      std::unique_ptr<views::Button> advanced_done_button);

  // Creates a translate icon for when the bottom branding isn't showing. This
  // should only be used on non-Chrome-branded builds.
  std::unique_ptr<views::ImageView> CreateTranslateIcon();

  // Creates a three dot options menu button.
  std::unique_ptr<views::Button> CreateOptionsMenuButton();

  // Creates a close button.
  std::unique_ptr<views::Button> CreateCloseButton();

  // Sets the window title. The window title still needs to be set, even when it
  // is not shown, for accessibility purposes.
  void SetWindowTitle(PartialTranslateBubbleModel::ViewState view_state);

  // Updates the view state. Whenever the view state is updated, the title needs
  // to be updated for accessibility.
  void UpdateViewState(PartialTranslateBubbleModel::ViewState view_state);

  // Switches the view type.
  void SwitchView(PartialTranslateBubbleModel::ViewState view_state);

  // Handles tab switching on when the view type switches.
  void SwitchTabForViewState(PartialTranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors::Type error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  // Actions for button presses shared with accelerators.
  void Translate();
  void ShowOriginal();
  void ConfirmAdvancedOptions();

  // Returns whether or not the current language selection is different from the
  // initial language selection in an advanced view.
  bool DidLanguageSelectionChange(
      PartialTranslateBubbleModel::ViewState view_state);

  // Handles the reset button in advanced view under Tab UI.
  void ResetLanguage();

  // Retrieve the names of the from/to languages and reset the language
  // indices.
  void UpdateLanguageNames(std::u16string* source_language_name,
                           std::u16string* target_language_name);

  void UpdateInsets(PartialTranslateBubbleModel::ViewState state);

  // Function bound to the "Translate full page" button.
  void TranslateFullPage();

  static PartialTranslateBubbleView* partial_translate_bubble_view_;

  raw_ptr<views::View> translate_view_ = nullptr;
  raw_ptr<views::View> error_view_ = nullptr;
  raw_ptr<views::View> advanced_view_source_ = nullptr;
  raw_ptr<views::View> advanced_view_target_ = nullptr;

  raw_ptr<views::Combobox> source_language_combobox_ = nullptr;
  raw_ptr<views::Combobox> target_language_combobox_ = nullptr;

  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;
  raw_ptr<views::View> tab_view_top_row_ = nullptr;
  raw_ptr<views::Label> partial_text_label_ = nullptr;

  raw_ptr<views::LabelButton> advanced_reset_button_source_ = nullptr;
  raw_ptr<views::LabelButton> advanced_reset_button_target_ = nullptr;
  raw_ptr<views::LabelButton> advanced_done_button_source_ = nullptr;
  raw_ptr<views::LabelButton> advanced_done_button_target_ = nullptr;

  // Default source/target language without user interaction.
  size_t previous_source_language_index_;
  size_t previous_target_language_index_;

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  std::unique_ptr<PartialTranslateBubbleModel> model_;

  translate::TranslateErrors::Type error_type_;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  std::u16string text_selection_;

  base::OnceClosure on_closing_;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_VIEW_H_
