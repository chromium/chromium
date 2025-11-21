// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox.mojom-shared.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_ui_types.h"
#include "url/gurl.h"

class OmniboxController;
class OmniboxPopupView;
class TemplateURL;
namespace gfx {
class Image;
}

class OmniboxEditModel {
  // TODO(crbug.com/392015004): Remove this macro once it gets fixed.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  struct State {
    State(bool user_input_in_progress,
          const std::u16string& user_text,
          const std::u16string& keyword,
          const std::u16string& keyword_placeholder,
          bool is_keyword_hint,
          metrics::OmniboxEventProto::KeywordModeEntryMethod
              keyword_mode_entry_method,
          OmniboxFocusState focus_state,
          const AutocompleteInput& autocomplete_input);
    State(const State& other);
    State& operator=(const State&) = delete;
    ~State();

    bool user_input_in_progress;
    const std::u16string user_text;
    const std::u16string keyword;
    const std::u16string keyword_placeholder;
    const bool is_keyword_hint;
    metrics::OmniboxEventProto::KeywordModeEntryMethod
        keyword_mode_entry_method;
    OmniboxFocusState focus_state;
    const AutocompleteInput autocomplete_input;
  };

  // Intended for view code to update itself when the model changes. Most view
  // updates are propagated by calling methods on `view_` & `popup_view_`
  // directly. But it's possible to have multiple views interested in being
  // updates (e.g. when the side-by-side debugging feature
  // `kWebUIOmniboxPopupDebug` is enabled). So updates that multiple views might
  // be interested in use the observer pattern. In the future, perhaps all
  // model->view updates can go through the observer.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the selection changes. The `line` field in either selection
    // may be `OmniboxPopupSelection::kNoMatch`.
    virtual void OnSelectionChanged(OmniboxPopupSelection old_selection,
                                    OmniboxPopupSelection new_selection) = 0;

    // Invoked when the icon used for the given match has been updated.
    virtual void OnMatchIconUpdated(size_t index) = 0;

    // Called when the results changed and the entire popup needs to be redrawn,
    // opened, or closed.
    virtual void OnContentsChanged() = 0;

    // The keyword state has changed.
    virtual void OnKeywordStateChanged(bool is_keyword_selected) = 0;

    ~Observer() override = default;
  };

  explicit OmniboxEditModel(OmniboxController* controller);
  OmniboxEditModel(const OmniboxEditModel&) = delete;
  OmniboxEditModel& operator=(const OmniboxEditModel&) = delete;
  virtual ~OmniboxEditModel();

  void set_view(OmniboxView* view) { view_ = view; }
  void set_popup_view(OmniboxPopupView* popup_view);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  metrics::OmniboxEventProto::PageClassification GetPageClassification() const;

  // Returns the current state.  This assumes we are switching tabs, and changes
  // the internal state appropriately.
  State GetStateForTabSwitch() const;

  // Resets the tab state, then restores local state from |state|. |state| may
  // be nullptr if there is no saved state.
  void RestoreState(const State* state);

  // Returns the match for the current text. If the user has not edited the text
  // this is the match corresponding to the permanent text. Returns the
  // alternate nav URL, if |alternate_nav_url| is non-NULL and there is such a
  // URL. Virtual for testing.
  virtual AutocompleteMatch CurrentMatchAndAlternateNavUrl(
      GURL* alternate_nav_url) const;

  // Provided for convenience, since most CurrentMatchAndAlternateNavUrl()
  // callers do not need the alternate navigation URL.
  AutocompleteMatch CurrentMatch() const {
    return CurrentMatchAndAlternateNavUrl(nullptr);
  }

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url,
                           std::u16string* title,
                           gfx::Image* favicon);

  // Returns true if the current edit contents will be treated as a
  // URL/navigation, as opposed to a search.
  bool CurrentTextIsURL() const;

  // Adjusts the copied text before writing it to the clipboard. If the copied
  // text is a URL with the scheme elided, this method reattaches the scheme.
  // Copied text that looks like a search query will not be modified.
  //
  // |sel_min| gives the minimum of the selection, e.g. min(sel_start, sel_end).
  // |text| is the currently selected text, and may be modified by this method.
  // |url_from_text| is the GURL interpretation of the selected text, and may
  // be used for drag-and-drop models or writing hyperlink data types to
  // system clipboards.
  //
  // If the copied text is interpreted as a URL:
  //  - |write_url| is set to true.
  //  - |url_from_text| is set to the URL.
  //  - |text| is set to the URL's spec. The output will be pure ASCII and
  //    %-escaped, since canonical URLs are always encoded to ASCII.
  //
  // If the copied text is *NOT* interpreted as a URL:
  //  - |write_url| is set to false.
  //  - |url_from_text| may be modified, but might not contain a valid GURL.
  //  - |text| is full UTF-16 and not %-escaped. This is because we are not
  //    interpreting |text| as a URL, so we leave the Unicode characters as-is.
  void AdjustTextForCopy(int sel_min,
                         std::u16string* text,
                         GURL* url_from_text,
                         bool* write_url);

  bool user_input_in_progress() const { return user_input_in_progress_; }

  std::u16string user_text() const { return user_text_; }

  // Encapsulates all the varied conditions for whether to override the
  // permanent page icon (associated with the currently displayed page),
  // with a temporary icon (associated with the current match or user text).
  bool ShouldShowCurrentPageIcon() const;

  // Returns the SuperGIcon for chrome builds. Otherwise return an empty
  // ImageModel. If `dark_mode` is enabled, return the monochrome version of the
  // icon.
  ui::ImageModel GetSuperGIcon(int image_size, bool dark_mode) const;

  // Whether the "Add Context" button should be shown in place of the location
  // bar page info icon button.
  bool ShouldShowAddContextButton() const;

  // Returns the "mega plus" icon associated with the "Add Context" button.
  ui::ImageModel GetAddContextIcon(int image_size) const;

  // Returns the Agentspace icon for chrome builds. Otherwise return an empty
  // Image. If `dark_mode` is enabled, return the monochrome version of the
  // icon.
  gfx::Image GetAgentspaceIcon(bool dark_mode) const;

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Calls SetInputInProgress, via SetInputInProgressNoNotify and
  // NotifyObserversInputInProgress, calling the latter after
  // StartAutocomplete, so that the result is only updated once.
  void UpdateInput(bool has_selected_text, bool prevent_inline_autocomplete);

  // Resets the permanent display texts (display_text_ and url_for_editing_)
  // to those provided by the controller. Returns true if the display texts
  // have changed and the change should be immediately user-visible, because
  // either the user is not editing or the edit does not have focus.
  bool ResetDisplayTexts();

  // Returns the permanent display text for the current page and Omnibox state.
  std::u16string GetPermanentDisplayText() const;

  // Sets the user_text_ to |text|. Also enters user-input-in-progress mode.
  // Virtual for testing.
  virtual void SetUserText(const std::u16string& text);

  // If the omnibox is currently displaying elided text, this method will
  // restore the full URL into the user text. After unelision, this selects-all,
  // enters user-input-in-progress mode, and then returns true.
  //
  // If the omnibox is not currently displaying elided text, this method will
  // no-op and return false.
  bool Unelide();

  // Invoked any time the text may have changed in the edit. Notifies the
  // controller.
  void OnChanged();

  // Reverts the edit model back to its unedited state (permanent text showing,
  // no user input in progress).
  void Revert();

  // Directs the popup to start autocomplete.  Makes use of the |view_| text and
  // selection, so make sure to set those before calling StartAutocomplete().
  void StartAutocomplete(bool has_selected_text,
                         bool prevent_inline_autocomplete);

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const std::u16string& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo(
      const std::u16string& text,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Sets |match| and |alternate_nav_url| based on classifying |text|.
  // |alternate_nav_url| may be nullptr.
  void ClassifyString(const std::u16string& text,
                      AutocompleteMatch* match,
                      GURL* alternate_nav_url) const;

  // Navigates to AI Mode, with the contents of the currently selected match, if
  // any.
  // `via_keyboard` is set to `true` if AI Mode was invoked via keyboard event
  // and is set to `false` if AI Mode was invoked via mouse / gesture event.
  // Virtual for testing.
  // `via_context_menu` is used to differentiate between users that open
  // the popup via the AI mode button vs context menu and allow for the popup
  // to open rather than navigate to the Google AI page when context is added.
  virtual void OpenAiMode(bool via_keyboard, bool via_context_menu);

  // Returns true if the popup is open and is in in AI-Mode.
  bool PopupInAiMode() const;

  // Opens given selection. Most kinds of selection invoke an action or
  // otherwise call `OpenMatch`, but some may `AcceptInput` which is not
  // guaranteed to open a match or commit the omnibox.
  // `via_keyboard` is set to `true` if the selection was opened due to a
  // keyboard event and is set to `false` if the selection was opened due
  // to a mouse / gesture event.
  void OpenSelection(
      OmniboxPopupSelection selection,
      base::TimeTicks timestamp = base::TimeTicks(),
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      bool via_keyboard = false);

  // A simplified version of `OpenSelection()` that opens the model's current
  // selection.
  void OpenSelectionForTesting(
      base::TimeTicks timestamp = base::TimeTicks(),
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      bool via_keyboard = false);

  OmniboxFocusState focus_state() const { return focus_state_; }
  bool has_focus() const { return focus_state_ != OMNIBOX_FOCUS_NONE; }

  base::TimeTicks last_omnibox_focus() const { return last_omnibox_focus_; }

  // This is the same as when the Omnibox is visibly focused.
  bool is_caret_visible() const {
    return focus_state_ == OMNIBOX_FOCUS_VISIBLE;
  }

  // Accessors for keyword-related state (see comments on `keyword_`,
  // `keyword_placeholder_` and `is_keyword_hint_`).
  const std::u16string& keyword() const { return keyword_; }
  const std::u16string& keyword_placeholder() const {
    return keyword_placeholder_;
  }
  bool is_keyword_hint() const { return is_keyword_hint_; }
  bool is_keyword_selected() const {
    return !is_keyword_hint_ && !keyword_.empty();
  }

  // Accepts the current keyword hint as a keyword. `entry_method` indicates how
  // the user entered keyword mode.
  void AcceptKeyword(
      metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method);

  // Sets the current keyword to that of the given `template_url` and updates
  // the view so the user sees the keyword chip in the omnibox.  Adjusts
  // user_text_ and the selection based on the display text and the keyword
  // entry method. Note, the default match may be updated in this process
  // so the match must support this keyword mode or it will be exited.
  void EnterKeywordMode(
      metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method,
      const TemplateURL* template_url,
      const std::u16string& placeholder_text);

  // Enters keyword mode for user's default search provider, if enabled.
  void EnterKeywordModeForDefaultSearchProvider(
      metrics::OmniboxEventProto::KeywordModeEntryMethod entry_method);

  // Accepts the current temporary text as the user text.
  void AcceptTemporaryTextAsUserText();

  // Clears the current keyword.
  void ClearKeyword();

  // Clears additional text.
  void ClearAdditionalText();

  // Called when the view is gaining focus.  |control_down| is whether the
  // control key is down (at the time we're gaining focus).
  void OnSetFocus(bool control_down);

  // Starts a request for zero-prefix suggestions if no query is currently
  // running and the popup is closed. This can be called multiple times without
  // harm, since it will early-exit if an earlier request is in progress or
  // done.
  void StartZeroSuggestRequest(bool user_clobbered_permanent_text = false);

  // Sets the visibility of the caret in the omnibox, if it has focus. The
  // visibility of the caret is reset to visible if either
  //   - The user starts typing, or
  //   - We explicitly focus the omnibox again.
  // The latter case must be handled in three separate places--OnSetFocus(),
  // OmniboxView::SetFocus(), and the mouse handlers in OmniboxView. See
  // accompanying comments for why each of these is necessary.
  //
  // Caret visibility is tracked per-tab and updates automatically upon
  // switching tabs.
  void SetCaretVisibility(bool visible);

  // If the ctrl key is down, marks it as consumed to prevent it from triggering
  // ctrl-enter behavior unless it is released and re-pressed.
  void ConsumeCtrlKey();

  // Sent before |OnKillFocus| and before the popup is closed.
  void OnWillKillFocus();

  // Called when the view is losing focus.  Resets some state.
  void OnKillFocus();

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text.
  void OnPaste();

  // Called when the user presses arrow up, arrow down, page up, or page down.
  void OnUpOrDownPressed(bool down, bool page);

  // Called when the user presses tab or shift+tab. The latter will traverse up
  // the selections instead of down.
  void OnTabPressed(bool shift);

  // Called when the user presses the space key without modifiers. Handles the
  // special keyword entry case where the user types '@bookmarks<space>'. Since
  // instant keywords don't have hints and are never the default match, this
  // need special treatment compared to when the user types
  // 'youtube.com<space>', which is handled by
  // `ShouldAcceptKeywordAfterInsertingSpaceAtEnd()`.
  bool OnSpacePressed();

  // Called when any relevant data changes.  This rolls together several
  // separate pieces of data into one call so we can update all the UI
  // efficiently. Specifically, it's invoked for temporary text, autocompletion,
  // and keyword changes.
  //   `temporary_text` is the new temporary text from the user selecting a
  //     different match. This will be empty when selecting a suggestion
  //     without a `fill_into_edit` (e.g. FOCUSED_BUTTON_HEADER) and when
  //     `is_temporary_test` is false.
  //   `is_temporary_text` is true if invoked because of a temporary text change
  //     or false if `temporary_text` should be ignored.
  //   `inline_autocompletion` is the autocompletion.
  //   `destination_for_temporary_text_change` is NULL (if temporary text should
  //     not change) or the pre-change destination URL (if temporary text should
  //     change) so we can save it off to restore later.
  //   `keyword` is the keyword to show a hint for if `is_keyword_hint` is true,
  //     or the currently selected keyword if `is_keyword_hint` is false (see
  //     comments on keyword_ and is_keyword_hint_).
  //   `additional_text` is additional omnibox text to be displayed adjacent to
  //     the omnibox view.
  //   `new_match` is the selected match when the user is changing selection,
  //     the default match if the user is typing, or an empty match when
  //     selecting a header.
  // Virtual to allow testing.
  virtual void OnPopupDataChanged(const std::u16string& temporary_text,
                                  bool is_temporary_text,
                                  const std::u16string& inline_autocompletion,
                                  const std::u16string& keyword,
                                  const std::u16string& keyword_placeholder,
                                  bool is_keyword_hint,
                                  const std::u16string& additional_text,
                                  const AutocompleteMatch& new_match);

  // Called by the `OmniboxView` after something changes, with details about
  // what state changes occurred. Updates internal state, updates the popup if
  // necessary, and returns true if any significant changes occurred. Note that
  // `text_change.text_differs` may be set even if `text_change.old_text` ==
  // `text_change.new_text`, e.g. if we've just committed an IME composition.
  // If `allow_keyword_ui_change` is false then the change should not affect
  // keyword UI state, even if the text matches a keyword exactly. This value
  // may be false when the user is composing a text with an IME.
  bool OnAfterPossibleChange(const OmniboxView::StateChanges& state_changes,
                             bool allow_keyword_ui_change);

  // Called when the current match has changed in the OmniboxController.
  void OnCurrentMatchChanged();

  AutocompleteInput GetInputForTesting() const { return input_; }

  // Name of the histogram tracking cut or copy omnibox commands.
  static const char kCutOrCopyAllTextHistogram[];

  // Just forwards the call to the OmniboxView referred within.
  void SetAccessibilityLabel(const AutocompleteMatch& match);

  // Reverts the edit box from a temporary text back to the original user text.
  // Also resets the popup to the initial state.
  void RevertTemporaryTextAndPopup();

  // Returns true if the destination URL of the match is bookmarked.
  bool IsStarredMatch(const AutocompleteMatch& match) const;

  // Gets the icon for the given `match`.
  gfx::Image GetMatchIcon(const AutocompleteMatch& match,
                          SkColor vector_icon_color,
                          bool dark_mode = false) const;
  // Gets the icon for the given `match` if the match was provided by an omnibox
  // API extension, otherwise returns empty image.
  gfx::Image GetMatchIconIfExtension(const AutocompleteMatch& match) const;

  // Gets the suggestion group header text associated with the given suggestion
  // group ID.
  // In addition to calling `AutocompleteResult::GetHeaderForSuggestionGroup()`,
  // this function takes into account certain header visibility criteria (e.g.
  // experiment flags) to determine the proper header text, which will then be
  // used by the relevant code to conditionally show suggestion group headers
  // in the Omnibox/Realbox popup.
  std::u16string GetSuggestionGroupHeaderText(
      const std::optional<omnibox::GroupId>& suggestion_group_id) const;

  // Called when the user hits escape after arrowing around the popup.  This
  // will reset the popup to the initial state.
  void ResetPopupToInitialState();

  // Gets popup's current selection.
  OmniboxPopupSelection GetPopupSelection() const;

  // Sets the current popup selection to |new_selection|. Caller is responsible
  // for making sure |new_selection| is valid. This assumes the popup is open.
  // This will update all state and repaint the necessary parts of the window,
  // as well as updating the textfield with the new temporary text.
  // |reset_to_default| restores the original inline autocompletion.
  // |force_update_ui| updates the UI even if the selection has not changed.
  void SetPopupSelection(OmniboxPopupSelection new_selection,
                         bool reset_to_default = false,
                         bool force_update_ui = false);

  // Returns true if popup selection is on the initial line, which is usually
  // the default match (except in the no-default-match case).
  bool IsPopupSelectionOnInitialLine() const;

  // Returns true if the control represented by |selection.state| is present on
  // the match in |selection.line|. This is the source-of-truth the UI code
  // should query to decide whether or not to draw the control.
  bool IsPopupControlPresentOnMatch(OmniboxPopupSelection selection) const;

  // From popup, tries to erase the suggestion at |line|. This should determine
  // if the item at |line| can be removed from history, and if so, remove it
  // and update the popup.
  void TryDeletingPopupLine(size_t line);

  // Returns the popup's accessibility label for current selection. This is an
  // extended version of AutocompleteMatchType::ToAccessibilityLabel() which
  // also returns narration about the any focused secondary button.
  // Never call this when the current selection is kNoMatch.
  std::u16string GetPopupAccessibilityLabelForCurrentSelection(
      const std::u16string& match_text,
      bool include_positional_info,
      int* label_prefix_length = nullptr);

  std::u16string GetPopupAccessibilityLabelForAimButton();

  // The IPH message that sometimes appears at the bottom of the Omnibox is
  // informational only and cannot be selected/focused. Its a11y label therefore
  // has to be read at the end of the last suggestion.  Returns the label for
  // the IPH row if the current selection is the one right before the IPH row.
  // Otherwise, returns an empty string.
  std::u16string MaybeGetPopupAccessibilityLabelForIPHSuggestion();

  // Invoked any time the result set of the controller changes.
  // TODO(orinj): This method seems like a good candidate for removal; it is
  // preserved here only to prevent possible behavior change while refactoring.
  void OnPopupResultChanged();

  // Lookup the bitmap for |result_index|. Returns nullptr if not found.
  const SkBitmap* GetPopupRichSuggestionBitmap(int result_index) const;

  // Lookup the bitmap based on the image URL.  Similar to above,
  // but used to fetch bitmap where the `result_index` is unknown and where
  // there are possibly multiple suggestions with the same `keyword` but not the
  // `image_url` being looked up. Returns nullptr if not found.
  const SkBitmap* GetPopupRichSuggestionBitmap(const GURL& image_url) const;

  // Lookup the icon bitmap based on the icon URL. Returns nullptr
  // if not found.
  const SkBitmap* GetIconBitmap(const GURL& icon_url) const;

  // Stores the image in a local data member and schedules a repaint.
  void SetPopupRichSuggestionBitmap(int result_index, const SkBitmap& bitmap);

  // Stores the icon in a local data member and schedules a repaint.
  void SetIconBitmap(const GURL& icon_url, const SkBitmap& bitmap);

  void SetAutocompleteInput(AutocompleteInput input);

  // Called to indicate a navigation may occur based on
  // |navigation_predictor| to the suggestion on |line|.
  void OnNavigationLikely(
      size_t line,
      omnibox::mojom::NavigationPredictor navigation_predictor);

 protected:
  // Utility method to get current PrefService; protected instead of private
  // because it may be overridden by derived test classes.
  virtual PrefService* GetPrefService();
  virtual const PrefService* GetPrefService() const;

 private:
  friend class OmniboxControllerTest;
  friend class TestOmniboxEditModel;
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKey);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKeyOnRequestFocus);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKeyOnCtrlAction);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxEditModelPopupTest,
      GetPopupRichSuggestionBitmapForMatchWithoutAssociatedKeyword);
  FRIEND_TEST_ALL_PREFIXES(
      OmniboxEditModelPopupTest,
      GetPopupRichSuggestionBitmapForMatchWithAssociatedKeyword);

  enum class PasteState {
    kNone,     // Most recent edit was not a paste.
    kPasting,  // In the middle of doing a paste. We need this intermediate
               // state because `OnPaste()` does the actual detection of paste,
               // but `OnAfterPossibleChange()` has to update the paste state
               // for every edit. If `OnPaste()` set the state directly to
               // kPasted, `OnAfterPossibleChange()` wouldn't know whether that
               // represented the current edit or a past one.
    kPasted,   // Most recent edit was a paste.
  };

  enum class ControlKeyState {
    kUp,              // The control key is not depressed.
    kDown,            // The control key is depressed and should trigger the
                      // "ctrl-enter" behavior when the user hits enter.
    kDownAndConsumed  // The control key is depressed, but has been consumed
                      // and should not trigger the "ctrl-enter" behavior.
                      // The control key becomes consumed if it has been used
                      // for another action such as focusing the location bar
                      // with ctrl-l or copying the selected text with ctrl-c.
  };

  AutocompleteController* autocomplete_controller() const;

  // If no query is in progress, starts working on an autocomplete query.
  // Returns true if started; false otherwise.
  bool MaybeStartQueryForPopup();

  // Changes the popup selection to the next available selection. Stepping the
  // popup selection gives special consideration for keyword mode state.
  void StepPopupSelection(OmniboxPopupSelection::Direction direction,
                          OmniboxPopupSelection::Step step);

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup.
  void AcceptInput(
      WindowOpenDisposition disposition,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Asks the browser to load |match| or execute one of its actions
  // according to |selection|.
  //
  // OpenMatch() needs to know the original text that drove this action.  If
  // |pasted_text| is non-empty, this is a Paste-And-Go/Search action, and
  // that's the relevant input text.  Otherwise, the relevant input text is
  // either the user text or the display URL, depending on if user input is
  // in progress.
  //
  // |match| is passed by value for two reasons:
  // (1) This function needs to modify |match|, so a const ref isn't
  //     appropriate.  Callers don't actually care about the modifications, so a
  //     pointer isn't required.
  // (2) The passed-in match is, on the caller side, typically coming from data
  //     associated with the popup.  Since this call can close the popup, that
  //     could clear that data, leaving us with a pointer-to-garbage.  So at
  //     some point someone needs to make a copy of the match anyway, to
  //     preserve it past the popup closure.
  void OpenMatch(OmniboxPopupSelection selection,
                 AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const std::u16string& pasted_text,
                 base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Updates the feedback type on the match at the given index and schedules a
  // repaint to update the suggestion view. On negative feedback, also shows the
  // feedback form.
  void UpdateFeedbackOnMatch(size_t match_index, FeedbackType feedback_type);

  // An internal method to set the user text. Notably, this differs from
  // SetUserText because it does not change the user-input-in-progress state.
  void InternalSetUserText(const std::u16string& text);

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  std::u16string MaybeStripKeyword(const std::u16string& text) const;
  std::u16string MaybePrependKeyword(const std::u16string& text) const;

  // Copies a match corresponding to the current text into |match|, and
  // populates |alternate_nav_url| as well if it's not nullptr. If the popup
  // is closed, the match is generated from the autocomplete classifier.
  void GetInfoForCurrentText(AutocompleteMatch* match,
                             GURL* alternate_nav_url) const;

  // Checks whether keyword mode space-triggering has been disabled either by
  // a pref or relevant feature flags. If the setting isn't available on the
  // search engines page, the pref should be ignored.  Returns true if space
  // triggering is enabled, false otherwise.
  bool AllowKeywordSpaceTriggering() const;

  // Whether the user just typed a space at the end of a keyword, with or
  // without inline autocompletion:
  // - youtube| -> youtube |       (a space was appended to a keyword)
  // - youtube[.com] -> youtube |  (a space replaced other text after a keyword)
  // Returns false when pressing space at the end of a keyword *prefix*:
  // - youtub[e.com] -> youtub |
  bool ShouldAcceptKeywordAfterInsertingSpaceAtEnd(
      const std::u16string& new_text);

  // Whether the user inserted a space into `old_text` and by doing so created a
  // `new_text` that looks like "<keyword> <search phrase>":
  // - youtube|query -> youtube |query
  bool ShouldAcceptKeywordAfterInsertingSpaceInMiddle(
      std::u16string_view old_text,
      std::u16string_view new_text,
      size_t caret_position) const;

  // Checks if a given character is a valid space character for accepting
  // keyword.
  static bool IsSpaceCharForAcceptingKeyword(wchar_t c);

  // Sets the state of user_input_in_progress_. Returns whether said state
  // changed, so that the caller can evoke NotifyObserversInputInProgress().
  bool SetInputInProgressNoNotify(bool in_progress);

  // Notifies the observers that the state has changed.
  void NotifyObserversInputInProgress(bool in_progress);

  // If focus_state_ does not match |state|, we update it and notify the
  // InstantController about the change (passing along the |reason| for the
  // change). If the caret visibility changes, we call ApplyCaretVisibility() on
  // the view.
  void SetFocusState(OmniboxFocusState state, OmniboxFocusChangeReason reason);

  // This is an event handler that notifies the popup view of match icon
  // changes.
  void OnFaviconFetched(const GURL& page_url, const gfx::Image& icon) const;

  // Returns view text if there is a view. Until the model is made the
  // primary data source, this should not be called when there's no view.
  std::u16string GetText() const;

  // Always use these to set keyword members instead of mutating them directly.
  void SetKeyword(const std::u16string& keyword);
  void SetKeywordPlaceholder(const std::u16string& keyword_placeholder);
  void SetIsKeywordHint(bool is_keyword_hint);

  // Record various UMA metrics associated with the AIM page action.
  // `query_text` represents the text entered by the user at activation time.
  // `activated` represents whether or not the user activated the page action.
  // `via_keyboard` represents the page action entry method (i.e. `true` =
  // keyboard event / `false` = mouse/gesture event).
  void RecordAiModeMetrics(const std::u16string& query_text,
                           bool activated,
                           bool via_keyboard);

  // Owns this.
  const raw_ptr<OmniboxController> controller_;

  // Owned by LocationBarView that owns the OmniboxController. It is cleared and
  // destroyed before OmniboxController is.
  raw_ptr<OmniboxView> view_;

  OmniboxFocusState focus_state_ = OMNIBOX_FOCUS_NONE;

  // Display-only text representing the current page. This could either:
  //  - The same as |url_for_editing_| if Steady State Elisions is OFF.
  //  - A simplified version of |url_for_editing_| with some destructive
  //    elisions applied. This is the case if Steady State Elisions is ON.
  //
  // This should not be considered suitable for editing.
  std::u16string display_text_;

  // The initial text representing the current URL suitable for editing.
  // This should fully represent the current URL without any meaning-changing
  // elisions applied - and is suitable for user editing.
  std::u16string url_for_editing_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag *should* be true in a superset of the cases where the popup is
  // open. Except (crbug.com/1340378) for zero suggestions when the popup was
  // opened with ctrl+L or a mouse click (as opposed to the down arrow).
  bool user_input_in_progress_ = false;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.  |user_text_| can
  // contain a string without |user_input_in_progress_| being true.
  // For instance, this is the case when the user has unelided a URL without
  // modifying its contents.
  std::u16string user_text_;

  // Used to know what should be displayed. Updated when e.g. the popup
  // selection changes, the results change, on navigation, on tab switch etc; it
  // should always be up-to-date.
  AutocompleteMatch current_match_;

  // We keep track of when the user last focused on the omnibox.
  base::TimeTicks last_omnibox_focus_;

  // Indicates whether the current interaction with the Omnibox resulted in
  // navigation (true), or user leaving the omnibox without taking any action
  // (false).
  // The value is initialized when the Omnibox receives focus and available for
  // use when the focus is about to be cleared.
  bool focus_resulted_in_navigation_ = false;

  // We keep track of when the user began modifying the omnibox text.
  // This should be valid whenever user_input_in_progress_ is true.
  base::TimeTicks time_user_first_modified_omnibox_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocompletion_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_ = false;
  std::u16string inline_autocompletion_;

  // Used by OnPopupDataChanged to keep track of whether there is currently a
  // temporary text.
  //
  // Example of use: If the user types "goog", then arrows down in the
  // autocomplete popup until, say, "google.com" appears in the edit box, then
  // the user_text_ is still "goog", and "google.com" is "temporary text".
  // When the user hits <esc>, the edit box reverts to "goog".  Hit <esc> again
  // and the popup is closed and "goog" is replaced by the permanent display
  // URL, which is the URL of the current page.
  //
  // original_user_text_with_keyword_ tracks the user_text_ before keywords are
  // removed. When accepting a keyword (from either a default match or another
  // lower in the dropdown), the user_text_ is destructively trimmed of its 1st
  // word. In order to restore the user_text_ properly when the omnibox reverts,
  // e.g. by pressing <escape> or pressing <up> until the first result is
  // selected, we track original_user_text_with_keyword_.
  // original_user_text_with_keyword_ is null if a keyword has not been
  // accepted.
  bool has_temporary_text_ = false;
  std::u16string original_user_text_with_keyword_;

  // When the user's last action was to paste, we disallow inline autocomplete
  // (on the theory that the user is trying to paste in a new URL or part of
  // one, and in either case inline autocomplete would get in the way).
  PasteState paste_state_ = PasteState::kNone;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_ = ControlKeyState::kUp;

  // The keyword associated with the current match.  The user may have an actual
  // selected keyword, or just some input text that looks like a keyword (so we
  // can show a hint to press <tab>).  This is the keyword in either case;
  // is_keyword_hint_ (below) distinguishes the two cases.
  std::u16string keyword_;

  // The placeholder text displayed for the keyword the user has selected.
  // Usually empty. Only used when the user input is empty.
  std::u16string keyword_placeholder_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_ = false;

  // Indicates how the user entered keyword mode if the user is actually in
  // keyword mode.  Otherwise, the value of this variable is INVALID.  This
  // is used to restore the user's search terms upon a call to ClearKeyword().
  metrics::OmniboxEventProto::KeywordModeEntryMethod
      keyword_mode_entry_method_ = metrics::OmniboxEventProto::INVALID;

  // This is needed to properly update the SearchModel state when the user
  // presses escape.
  bool in_revert_ = false;

  // Indicates if the upcoming autocomplete search is allowed to be treated as
  // an exact keyword match.  If this is true then keyword mode will be
  // triggered automatically if the input is "<keyword> <search string>".  We
  // allow this when `ShouldAcceptKeywordAfterInsertingSpaceInMiddle()` is true.
  // This has no effect if we're already in keyword mode.
  bool allow_exact_keyword_match_ = false;

  // The input that was sent to the AutocompleteController. Since no
  // autocomplete query is started after a tab switch, it is possible for this
  // |input_| to differ from the one currently stored in AutocompleteController.
  AutocompleteInput input_;

  // Rich suggestion bitmaps for popup keyed by `result_index`. These are
  // cleared when `OmniboxPopupViewViews` is initialized and destroyed, and on
  // `OnPopupResultChanged()`.
  std::map<int, SkBitmap> rich_suggestion_bitmaps_;

  // Icon bitmaps for popup keyed by `icon_url`. These are cleared when
  // `OmniboxPopupViewViews` is initialized and destroyed. This differs from
  // `rich_suggestion_bitmaps_` since they are not cleared on
  // `OnPopupResultChanged()`, which allows for fetching the icon even when the
  // popup is closed.
  std::map<GURL, SkBitmap> icon_bitmaps_;

  // The popup view is nullptr when there's no popup, and is non-null when
  // a popup view exists (i.e. between calls to `set_popup_view`).
  raw_ptr<OmniboxPopupView> popup_view_ = nullptr;

  // The current popup selection; set to normal kNoMatch when there's no popup.
  OmniboxPopupSelection popup_selection_ =
      OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                            OmniboxPopupSelection::NORMAL);

  // When a result changes, this informs of the URL in the previously selected
  // suggestion whose tab switch button was focused, so that we may compare
  // if equal.
  GURL old_focused_url_;

  // See comment on `Observer`.
  mutable base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<OmniboxEditModel> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_
