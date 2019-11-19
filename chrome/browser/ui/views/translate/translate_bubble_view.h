// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/source_language_combobox_model.h"
#include "chrome/browser/ui/translate/target_language_combobox_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/non_client_view.h"

class Browser;

namespace views {
class Checkbox;
class LabelButton;
class View;
}  // namespace views

class TranslateBubbleView : public LocationBarBubbleDelegateView,
                            public views::ButtonListener,
                            public views::ComboboxListener,
                            public views::LinkListener,
                            public ui::SimpleMenuModel::Delegate,
                            public views::StyledLabelListener,
                            public views::TabbedPaneListener {
 public:
  // Item IDs for the option button's menu.
  enum OptionsMenuItem {
    ALWAYS_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_SITE,
    MORE_OPTIONS,
    CHANGE_TARGET_LANGUAGE,
    CHANGE_SOURCE_LANGUAGE
  };

  ~TranslateBubbleView() override;

  // Shows the Translate bubble. Returns the newly created bubble's Widget or
  // nullptr in cases when the bubble already exists or when the bubble is not
  // created.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  static views::Widget* ShowBubble(views::View* anchor_view,
                                   views::Button* highlighted_button,
                                   content::WebContents* web_contents,
                                   translate::TranslateStep step,
                                   const std::string& source_language,
                                   const std::string& target_language,
                                   translate::TranslateErrors::Type error_type,
                                   DisplayReason reason);

  // Closes the current bubble if it exists.
  static void CloseCurrentBubble();

  // Returns the bubble view currently shown. This may return NULL.
  static TranslateBubbleView* GetCurrentBubble();

  TranslateBubbleModel* model() { return model_.get(); }

  // LocationBarBubbleDelegateView:
  base::string16 GetWindowTitle() const override;
  void Init() override;
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  void WindowClosing() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetClosing(views::Widget* widget) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Returns the current view state.
  TranslateBubbleModel::ViewState GetViewState() const;

 protected:
  // LocationBarBubbleDelegateView:
  void CloseBubble() override;

 private:
  enum LinkID {
    LINK_ID_ADVANCED,
  };

  enum ButtonID {
    BUTTON_ID_TRANSLATE,
    BUTTON_ID_DONE,
    BUTTON_ID_CANCEL,
    BUTTON_ID_SHOW_ORIGINAL,
    BUTTON_ID_TRY_AGAIN,
    BUTTON_ID_ALWAYS_TRANSLATE,
    BUTTON_ID_ADVANCED,
    BUTTON_ID_OPTIONS_MENU,
    BUTTON_ID_OPTIONS_MENU_TAB,
    BUTTON_ID_CLOSE,
    BUTTON_ID_RESET,
    BUTTON_ID_RETURN
  };

  enum ComboboxID {
    COMBOBOX_ID_SOURCE_LANGUAGE,
    COMBOBOX_ID_TARGET_LANGUAGE,
  };

  friend class TranslateBubbleViewTest;
  friend void ::translate::test_utils::PressTranslate(::Browser*);
  friend void ::translate::test_utils::PressRevert(::Browser*);
  friend void ::translate::test_utils::SelectTargetLanguageByDisplayName(
      ::Browser*,
      const ::base::string16&);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TranslateButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TabUiTranslateButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, AdvancedLink);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, ShowOriginalButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TryAgainButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndCancelButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateCheckboxAndDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, DoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TabUiSourceDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, TabUiTargetDoneButton);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           DoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiSourceDoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiTargetDoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TabUiSourceTranslateBubbleViewTest,
                           DoneButtonWithoutTranslating);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningBeforeTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           CancelButtonReturningAfterTranslate);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest, CancelButtonReturningError);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiOptionsMenuNeverTranslateLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuRespectsBlacklistSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiOptionsMenuRespectsBlacklistSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           OptionsMenuNeverTranslateSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiOptionsMenuNeverTranslateSite);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           AlwaysTranslateLanguageMenuItem);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiAlwaysTranslateLanguageMenuItem);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiTabSelectedAfterTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewTest,
                           TabUiAlwaysTranslateTriggerTranslation);
  FRIEND_TEST_ALL_PREFIXES(TranslateLanguageBrowserTest, TranslateAndRevert);
  FRIEND_TEST_ALL_PREFIXES(TranslateBubbleViewBrowserTest,
                           CheckNeverTranslateThisSiteBlacklist);

  TranslateBubbleView(views::View* anchor_view,
                      std::unique_ptr<TranslateBubbleModel> model,
                      translate::TranslateErrors::Type error_type,
                      content::WebContents* web_contents);

  // views::TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // Returns the current child view.
  views::View* GetCurrentView() const;

  // Triggers options menu.
  void ShowOptionsMenu(views::Button* source);

  // Triggers options menu in TAB UI.
  void ShowOptionsMenuTab(views::Button* source);

  // Handles the event when the user clicks a link.
  void HandleLinkClicked(LinkID sender_id);

  // Handles the event when the user changes an index of a combobox.
  void HandleComboboxPerformAction(ComboboxID sender_id);

  // Updates the visibilities of child views according to the current view type.
  void UpdateChildVisibilities();

  // Creates the 'before translate' view.
  std::unique_ptr<views::View> CreateViewBeforeTranslate();

  // Creates the view for TAB UI. This view is being used before/during/after
  // translate.
  std::unique_ptr<views::View> CreateViewTab();

  // AddTab function requires a view element to be shown below each tab.
  // This function creates an empty view so no extra white space below the tab.
  std::unique_ptr<views::View> CreateEmptyPane();

  // Creates the 'translating' view.
  std::unique_ptr<views::View> CreateViewTranslating();

  // Creates the 'after translate' view.
  std::unique_ptr<views::View> CreateViewAfterTranslate();

  // Creates the 'error' view for Button UI. Caller takes ownership of the
  // returned view.
  std::unique_ptr<views::View> CreateViewError();

  // Creates the 'error' view skeleton UI with no title. Caller takes ownership
  // of the returned view.
  std::unique_ptr<views::View> CreateViewErrorNoTitle(
      std::unique_ptr<views::Button> advanced_button);

  // Creates the 'error' view for Tab and Button_GM2 UI.
  std::unique_ptr<views::View> CreateViewErrorTab();
  std::unique_ptr<views::View> CreateViewErrorGM2();

  // Creates the 'advanced' view. Caller takes ownership of the returned view.
  // Three options depending on UI selection in kUseButtonTranslateBubbleUI.
  std::unique_ptr<views::View> CreateViewAdvanced();

  // Creates source language label and combobox for Tab UI advanced view
  std::unique_ptr<views::View> TabUiCreateViewAdvanedSource();

  // Creates source language label and combobox for Tab UI advanced view
  std::unique_ptr<views::View> TabUiCreateViewAdvanedTarget();

  // Tab UI present the same view for before/during/after translate state.
  bool TabUiIsEquivalentState(TranslateBubbleModel::ViewState view_state);

  // Creates the skeleton view for GM2 UI.
  std::unique_ptr<views::View> GM2CreateView(
      std::unique_ptr<views::Button> action_button,
      std::unique_ptr<views::View> status_indicator,
      bool active_option_button,
      std::unique_ptr<views::Label> source_language_label,
      std::unique_ptr<views::Label> target_language_label);

  // Creates the 'before translate' view for Button_GM2 UI.
  std::unique_ptr<views::View> GM2CreateViewBeforeTranslate();

  // Creates the 'translating' view for Button_GM2 UI.
  std::unique_ptr<views::View> GM2CreateViewTranslating();

  // Creates the 'after translate' view for Button_GM2 UI.
  std::unique_ptr<views::View> GM2CreateViewAfterTranslate();

  // Creates the 'advanced' view to show source/target language combobox under
  // TAB UI. Caller takes ownership of the returned view.
  std::unique_ptr<views::View> CreateViewAdvancedTabUi(
      std::unique_ptr<views::Combobox> combobox,
      std::unique_ptr<views::Label> language_title_label);

  std::unique_ptr<views::Button> CreateCloseButton();

  // Get the current always translate checkbox
  views::Checkbox* GetAlwaysTranslateCheckbox();

  // Switches the view type.
  void SwitchView(TranslateBubbleModel::ViewState view_state);

  // SwitchView handler for TAB UI since TAB UI has the same view throughout.
  void SwitchTabForViewState(TranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors::Type error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  // Actions for button presses shared with accelerators.
  void Translate();
  void ShowOriginal();
  void ConfirmAdvancedOptions();

  // Handles the reset button in advanced view under Tab UI.
  void ResetLanguage();

  // Retrieve the names of the from/to languages and reset the language
  // indices.
  void UpdateLanguageNames(base::string16* original_language_name,
                           base::string16* target_language_name);

  void UpdateInsets(TranslateBubbleModel::ViewState state);

  static TranslateBubbleView* translate_bubble_view_;

  views::View* before_translate_view_ = nullptr;
  views::View* translating_view_ = nullptr;
  views::View* after_translate_view_ = nullptr;
  views::View* error_view_ = nullptr;
  views::View* advanced_view_ = nullptr;
  views::View* tab_translate_view_ = nullptr;
  views::View* advanced_view_source_ = nullptr;
  views::View* advanced_view_target_ = nullptr;

  std::unique_ptr<SourceLanguageComboboxModel> source_language_combobox_model_;
  std::unique_ptr<TargetLanguageComboboxModel> target_language_combobox_model_;

  views::Combobox* source_language_combobox_ = nullptr;
  views::Combobox* target_language_combobox_ = nullptr;

  views::Checkbox* before_always_translate_checkbox_ = nullptr;
  views::Checkbox* advanced_always_translate_checkbox_ = nullptr;
  views::TabbedPane* tabbed_pane_ = nullptr;

  // Button_GM2 UI source/target language label class variable to be updated
  // based on user selction in
  views::Label* gm2_source_language_label_ = nullptr;
  views::Label* gm2_target_language_label_ = nullptr;

  views::LabelButton* advanced_cancel_button_ = nullptr;
  views::LabelButton* advanced_done_button_ = nullptr;

  // Default source/target language without user interaction.
  int previous_source_language_index_;
  int previous_target_language_index_;

  // Used to trigger the options menu in tests.
  views::Button* before_translate_options_button_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  std::unique_ptr<ui::SimpleMenuModel> tab_options_menu_model_;

  std::unique_ptr<TranslateBubbleModel> model_;

  translate::TranslateErrors::Type error_type_;

  // Whether the window is an incognito window.
  const bool is_in_incognito_window_;

  const language::TranslateUIBubbleModel bubble_ui_model_;

  bool should_always_translate_ = false;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_BUBBLE_VIEW_H_
