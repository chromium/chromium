// Copyright 2022 The Chromium Authors
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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class Combobox;
class LabelButton;
class View;
}  // namespace views

class PartialTranslateBubbleView : public LocationBarBubbleDelegateView,
                                   public ui::SimpleMenuModel::Delegate,
                                   public views::TabbedPaneListener {
  METADATA_HEADER(PartialTranslateBubbleView, LocationBarBubbleDelegateView)

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
                             content::WebContents* web_contents,
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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns the current view state.
  PartialTranslateBubbleModel::ViewState GetViewState() const;

  // Initialize the bubble in the correct view state when it is shown.
  void SetViewState(PartialTranslateBubbleModel::ViewState view_state,
                    translate::TranslateErrors error_type);

  // Update the source language combobox's selected index to match the current
  // index in the model. These values desynchronize when a request does not
  // specify a source language, such as with initial translations from the menu,
  // or when "Detected Language" is used.
  void MaybeUpdateSourceLanguageCombobox();

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
                           TargetLanguageTabDoesntTriggerTranslate);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           TabSelectedAfterTranslation);
  FRIEND_TEST_ALL_PREFIXES(PartialTranslateBubbleViewTest,
                           UpdateLanguageTabsFromResponse);
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
      std::unique_ptr<views::Button> button);

  // Creates the 'waiting' view that shows an empty bubble with a throbber.
  std::unique_ptr<views::View> CreateViewWaiting();

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

  // Finds and saves the width of the bubble's largest child view, excluding
  // |translate_view_|. This value is needed to properly resize
  // |partial_text_label_| as the width of the tabbed pane changes with changes
  // in selected languages.
  void ComputeLargestViewStateWidth();

  // Updates the view state. Whenever the view state is updated, the title needs
  // to be updated for accessibility.
  void UpdateViewState(PartialTranslateBubbleModel::ViewState view_state);

  // Switches the view type.
  void SwitchView(PartialTranslateBubbleModel::ViewState view_state);

  // Handles tab switching on when the view type switches.
  void SwitchTabForViewState(PartialTranslateBubbleModel::ViewState view_state);

  // Switches to the error view.
  void SwitchToErrorView(translate::TranslateErrors error_type);

  // Updates the advanced view.
  void UpdateAdvancedView();

  // Actions for button presses shared with accelerators.
  void ShowTranslated();
  void ShowOriginal();
  void ConfirmAdvancedOptions();

  // Returns whether or not the current language selection is different from the
  // initial language selection in an advanced view.
  bool DidLanguageSelectionChange(
      PartialTranslateBubbleModel::ViewState view_state);

  // Handles the reset button in advanced view under Tab UI.
  void ResetLanguage();

  // Updates the body text for the bubble based on the view state (either the
  // source text if we're pre-translate or translating, or the target text if
  // we're done translating).
  void UpdateTextForViewState(
      PartialTranslateBubbleModel::ViewState view_state);

  // Update the names of the source/target language tabs.
  void UpdateLanguageTabNames();

  void UpdateInsets(PartialTranslateBubbleModel::ViewState state);

  // Function bound to the "Translate full page" button.
  void TranslateFullPage();

  // Update the alignment of |partial_text_label_| to match the direction of
  // the locale being used.
  void SetTextAlignmentForLocaleTextDirection(std::string locale);

  // Forces announcement of translation state and conditionally also accounces
  // the translated text.
  void AnnounceForAccessibility(
      PartialTranslateBubbleModel::ViewState view_state);

  static PartialTranslateBubbleView* partial_translate_bubble_view_;

  raw_ptr<views::View, DanglingUntriaged> translate_view_waiting_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> translate_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> error_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> advanced_view_source_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> advanced_view_target_ = nullptr;

  raw_ptr<views::Throbber, DanglingUntriaged> throbber_;

  raw_ptr<views::Combobox, DanglingUntriaged> source_language_combobox_ =
      nullptr;
  raw_ptr<views::Combobox, DanglingUntriaged> target_language_combobox_ =
      nullptr;

  raw_ptr<views::TabbedPane, DanglingUntriaged> tabbed_pane_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> tab_view_top_row_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> partial_text_label_ = nullptr;

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

  // Whether or not user changed target language and triggered translation from
  // the advanced options.
  bool target_language_changed_ = false;

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  std::unique_ptr<PartialTranslateBubbleModel> model_;

  translate::TranslateErrors error_type_;

  std::unique_ptr<WebContentMouseHandler> mouse_handler_;

  std::u16string text_selection_;

  // The width of the largest non-|translate_view_| child view at
  // initialization. The default minimum width is set to 300dp to provide a more
  // consistent experience between different UI languages - for high density
  // languages the width would otherwise be very narrow.
  int largest_view_state_width_ = 300;

  // The threshold for character volume of |partial_text_label_|. If the volume
  // is larger than the threshold, use the preset maximum width allowable for
  // the bubble. Otherwise, resize normally.
  const size_t char_threshold_for_max_width_ = 1300;

  // The max allowable width for the bubble, used when the character volume of
  // |partial_text_label_| exceeds |char_threshold_for_max_width_|. This is
  // necessary to accommodate the size of the label in cases of largest possible
  // character volume. It follows that this is based off of
  // translate::kDesktopPartialTranslateTextSelectionMaxCharacters and should be
  // updated alongside it.
  const int bubble_max_width_ = 550;

  base::OnceClosure on_closing_;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_VIEW_H_
