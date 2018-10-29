// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

class AutocompleteResult;
class OmniboxClient;
class OmniboxEditController;
class OmniboxPopupModel;
class OmniboxView;

namespace gfx {
class Image;
}

// Reasons why the Omnibox could change into keyword mode.
// These numeric values are used in UMA logs; do not change them.
enum class KeywordModeEntryMethod {
  TAB = 0,
  SPACE_AT_END = 1,
  SPACE_IN_MIDDLE = 2,
  KEYBOARD_SHORTCUT = 3,
  QUESTION_MARK = 4,
  CLICK_ON_VIEW = 5,
  TAP_ON_VIEW = 6,
  NUM_ITEMS,
};

class OmniboxEditModel {
 public:
  // Did the Omnibox focus originate via the user clicking on the Omnibox, on
  // the Fakebox or the Search button?
  enum FocusSource {
    INVALID = 0,
    OMNIBOX = 1,
    FAKEBOX = 2,
    SEARCH_BUTTON = 3
  };

  struct State {
    State(bool user_input_in_progress,
          const base::string16& user_text,
          const base::string16& keyword,
          bool is_keyword_hint,
          KeywordModeEntryMethod keyword_mode_entry_method,
          OmniboxFocusState focus_state,
          FocusSource focus_source,
          const AutocompleteInput& autocomplete_input);
    State(const State& other);
    ~State();

    bool user_input_in_progress;
    const base::string16 user_text;
    const base::string16 keyword;
    const bool is_keyword_hint;
    KeywordModeEntryMethod keyword_mode_entry_method;
    OmniboxFocusState focus_state;
    FocusSource focus_source;
    const AutocompleteInput autocomplete_input;
  };

  // This is a mirror of content::kMaxURLDisplayChars because ios cannot depend
  // on content. If clipboard contains more than kMaxPasteAndGoTextLength
  // characters, then the paste & go option will be disabled.
  static const size_t kMaxPasteAndGoTextLength = 32 * 1024;

  OmniboxEditModel(OmniboxView* view,
                   OmniboxEditController* controller,
                   std::unique_ptr<OmniboxClient> client);
  virtual ~OmniboxEditModel();

  // TODO(jdonnelly): Remove this accessor when the AutocompleteController has
  //     completely moved to OmniboxController.
  AutocompleteController* autocomplete_controller() const {
    return omnibox_controller_->autocomplete_controller();
  }

  void set_popup_model(OmniboxPopupModel* popup_model) {
    omnibox_controller_->set_popup_model(popup_model);
  }

  // TODO(jdonnelly): The edit and popup should be siblings owned by the
  // LocationBarView, making this accessor unnecessary.
  // NOTE: popup_model() can be NULL for testing.
  OmniboxPopupModel* popup_model() const {
    return omnibox_controller_->popup_model();
  }

  OmniboxEditController* controller() const { return controller_; }

  OmniboxClient* client() const { return client_.get(); }

  // Returns the current state.  This assumes we are switching tabs, and changes
  // the internal state appropriately.
  const State GetStateForTabSwitch();

  // Resets the tab state, then restores local state from |state|. |state| may
  // be nullptr if there is no saved state.
  void RestoreState(const State* state);

  // Returns the match for the current text. If the user has not edited the text
  // this is the match corresponding to the permanent text. Returns the
  // alternate nav URL, if |alternate_nav_url| is non-NULL and there is such a
  // URL. Virtual for testing.
  virtual AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const;

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url,
                           base::string16* title,
                           gfx::Image* favicon);

  // Returns true if the current edit contents will be treated as a
  // URL/navigation, as opposed to a search.
  bool CurrentTextIsURL() const;

  // Adjusts the copied text before writing it to the clipboard. If the copied
  // text is a URL with the scheme elided, this method reattaches the scheme.
  // Copied text that looks like a search query, including the Query in Omnibox
  // case, will not be modified.
  //
  // |sel_min| gives the minimum of the selection, e.g. min(sel_start, sel_end).
  // |text| is the currently selected text. If the copied text is interpreted
  // as a URL, |write_url| is set to true and |url_from_text| set to the URL.
  void AdjustTextForCopy(int sel_min,
                         base::string16* text,
                         GURL* url_from_text,
                         bool* write_url);

  bool user_input_in_progress() const { return user_input_in_progress_; }

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Calls SetInputInProgress, via SetInputInProgressNoNotify and
  // NotifyObserversInputInProgress, calling the latter after
  // StartAutocomplete, so that the result is only updated once.
  void UpdateInput(bool has_selected_text,
                   bool prevent_inline_autocomplete);

  // Resets the permanent display texts (display_text_ and url_for_editing_)
  // to those provided by the controller. Returns true if the display texts
  // have changed and the change should be immediately user-visible, because
  // either the user is not editing or the edit does not have focus.
  bool ResetDisplayTexts();

  // Returns the URL corresponding to the permanent text.
  GURL PermanentURL() const;

  // Returns the permanent display text for the current page and Omnibox state.
  base::string16 GetPermanentDisplayText() const;

  // Sets the user_text_ to |text|.
  void SetUserText(const base::string16& text);

  // Unapplies any Steady State Elisions by setting the user text to be
  // url_for_editing_. This also selects all and enters user-input-in-progress
  // mode. If |exit_query_in_omnibox| is set to true, this will alse exit
  // Query in Omnibox mode if the omnibox is showing a query.
  void Unelide(bool exit_query_in_omnibox);

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

  // Closes the popup and cancels any pending asynchronous queries.
  void StopAutocomplete();

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const base::string16& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo(
      const base::string16& text,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Returns true if |text| classifies as a Search rather than a URL.
  bool ClassifiesAsSearch(const base::string16& text) const;

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup. If |for_drop| is true,
  // it indicates the input is being accepted as part of a drop operation and
  // the transition should be treated as LINK (so that it won't trigger the
  // URL to be autocompleted).
  void AcceptInput(
      WindowOpenDisposition disposition,
      bool for_drop,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Asks the browser to load the item at |index|, with the given properties.
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
  void OpenMatch(AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t index,
                 base::TimeTicks match_selection_timestamp = base::TimeTicks());

  OmniboxFocusState focus_state() const { return focus_state_; }
  bool has_focus() const { return focus_state_ != OMNIBOX_FOCUS_NONE; }

  // This is the same as when the Omnibox is visibly focused.
  bool is_caret_visible() const {
    return focus_state_ == OMNIBOX_FOCUS_VISIBLE;
  }

  FocusSource focus_source() const { return focus_source_; }
  void set_focus_source(FocusSource focus_source) {
    focus_source_ = focus_source;
  }

  // Accessors for keyword-related state (see comments on keyword_ and
  // is_keyword_hint_).
  const base::string16& keyword() const { return keyword_; }
  bool is_keyword_hint() const { return is_keyword_hint_; }
  bool is_keyword_selected() const {
    return !is_keyword_hint_ && !keyword_.empty();
  }

  // Accepts the current keyword hint as a keyword. It always returns true for
  // caller convenience. |entered_method| indicates how the user entered
  // keyword mode.
  bool AcceptKeyword(KeywordModeEntryMethod entry_method);

  // Sets the current keyword to that of the user's default search provider and
  // updates the view so the user sees the keyword chip in the omnibox.  Adjusts
  // user_text_ and the selection based on the display text and the keyword
  // entry method.
  void EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod entry_method);

  // Accepts the current temporary text as the user text.
  void AcceptTemporaryTextAsUserText();

  // Clears the current keyword.
  void ClearKeyword();

  // Returns the current autocomplete result.  This logic should in the future
  // live in AutocompleteController but resides here for now.  This method is
  // used by AutomationProvider::AutocompleteEditGetMatches.
  const AutocompleteResult& result() const {
    return omnibox_controller_->result();
  }

  // Called when the view is gaining focus.  |control_down| is whether the
  // control key is down (at the time we're gaining focus).
  void OnSetFocus(bool control_down);

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

  // Returns whether the omnibox will handle a press of the escape key.  The
  // caller can use this to decide whether the browser should process escape as
  // "stop current page load".
  bool WillHandleEscapeKey() const;

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text.
  void OnPaste();

  // Returns true if pasting is in progress.
  bool is_pasting() const { return paste_state_ == PASTING; }

  // Called when the user presses up or down.  |count| is a repeat count,
  // negative for moving up, positive for moving down. Virtual for testing.
  virtual void OnUpOrDownKeyPressed(int count);

  // Called when any relevant data changes.  This rolls together several
  // separate pieces of data into one call so we can update all the UI
  // efficiently:
  //   |text| is either the new temporary text from the user manually selecting
  //     a different match, or the inline autocomplete text.  We distinguish by
  //     checking if |destination_for_temporary_text_change| is NULL.
  //   |destination_for_temporary_text_change| is NULL (if temporary text should
  //     not change) or the pre-change destination URL (if temporary text should
  //     change) so we can save it off to restore later.
  //   |keyword| is the keyword to show a hint for if |is_keyword_hint| is true,
  //     or the currently selected keyword if |is_keyword_hint| is false (see
  //     comments on keyword_ and is_keyword_hint_).
  void OnPopupDataChanged(
      const base::string16& text,
      GURL* destination_for_temporary_text_change,
      const base::string16& keyword,
      bool is_keyword_hint);

  // Called by the OmniboxView after something changes, with details about what
  // state changes occured.  Updates internal state, updates the popup if
  // necessary, and returns true if any significant changes occurred.  Note that
  // |text_change.text_differs| may be set even if |text_change.old_text| ==
  // |text_change.new_text|, e.g. if we've just committed an IME composition.
  //
  // If |allow_keyword_ui_change| is false then the change should not affect
  // keyword ui state, even if the text matches a keyword exactly. This value
  // may be false when the user is composing a text with an IME.
  bool OnAfterPossibleChange(const OmniboxView::StateChanges& state_changes,
                             bool allow_keyword_ui_change);

  // Called when the current match has changed in the OmniboxController.
  void OnCurrentMatchChanged();

  // Convenience method for QueryInOmnibox::GetDisplaySearchTerms.
  // Returns true if Query in Omnibox is active. |search_terms| may be nullptr.
  bool GetQueryInOmniboxSearchTerms(base::string16* search_terms) const;

  // Used for testing purposes only.
  base::string16 GetUserTextForTesting() const { return user_text_; }

  // Name of the histogram tracking cut or copy omnibox commands.
  static const char kCutOrCopyAllTextHistogram[];

  OmniboxView* view() { return view_; }

 private:
  friend class OmniboxControllerTest;
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKey);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKeyOnRequestFocus);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelTest, ConsumeCtrlKeyOnCtrlAction);

  enum PasteState {
    NONE,           // Most recent edit was not a paste.
    PASTING,        // In the middle of doing a paste. We need this intermediate
                    // state because OnPaste() does the actual detection of
                    // paste, but OnAfterPossibleChange() has to update the
                    // paste state for every edit. If OnPaste() set the state
                    // directly to PASTED, OnAfterPossibleChange() wouldn't know
                    // whether that represented the current edit or a past one.
    PASTED,         // Most recent edit was a paste.
  };

  enum ControlKeyState {
    UP,                // The control key is not depressed.
    DOWN,              // The control key is depressed and should trigger the
                       // "ctrl-enter" behavior when the user hits enter.
    DOWN_AND_CONSUMED  // The control key is depressed, but has been consumed
                       // and should not trigger the "ctrl-enter" behavior.
                       // The control key becomes consumed if it has been used
                       // for another action such as focusing the location bar
                       // with ctrl-l or copying the selected text with ctrl-c.
  };

  // Returns true if a query to an autocomplete provider is currently
  // in progress.  This logic should in the future live in
  // AutocompleteController but resides here for now.  This method is used by
  // AutomationProvider::AutocompleteEditIsQueryInProgress.
  bool query_in_progress() const { return !autocomplete_controller()->done(); }

  // Returns true if the popup exists and is open.  (This is a convenience
  // wrapper for the benefit of test code, which may not have a popup model.)
  // Virtual for testing.
  virtual bool PopupIsOpen() const;

  // Called whenever user_text_ should change.
  void InternalSetUserText(const base::string16& text);

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  base::string16 MaybeStripKeyword(const base::string16& text) const;
  base::string16 MaybePrependKeyword(const base::string16& text) const;

  // If there's a selected match, copies it into |match|. Else, returns the
  // default match for the current text, as well as the alternate nav URL, if
  // |alternate_nav_url| is non-NULL and there is such a URL.
  void GetInfoForCurrentText(AutocompleteMatch* match,
                             GURL* alternate_nav_url) const;

  // Reverts the edit box from a temporary text back to the original user text.
  // If |revert_popup| is true then the popup will be reverted as well.
  void RevertTemporaryText(bool revert_popup);

  // Accepts current keyword if the user just typed a space at the end of
  // |new_text|.  This handles both of the following cases:
  //   (assume "foo" is a keyword, | is the input caret, [] is selected text)
  //   foo| -> foo |      (a space was appended to a keyword)
  //   foo[bar] -> foo |  (a space replaced other text after a keyword)
  // Returns true if the current keyword is accepted.
  bool MaybeAcceptKeywordBySpace(const base::string16& new_text);

  // Checks whether the user inserted a space into |old_text| and by doing so
  // created a |new_text| that looks like "<keyword> <search phrase>".
  bool CreatedKeywordSearchByInsertingSpaceInMiddle(
      const base::string16& old_text,
      const base::string16& new_text,
      size_t caret_position) const;

  // Checks if a given character is a valid space character for accepting
  // keyword.
  static bool IsSpaceCharForAcceptingKeyword(wchar_t c);

  // Classify the current page being viewed as, for example, the new tab
  // page or a normal web page.  Used for logging omnibox events for
  // UMA opted-in users.  Examines the user's profile to determine if the
  // current page is the user's home page.
  metrics::OmniboxEventProto::PageClassification ClassifyPage() const;

  // Sets |match| and |alternate_nav_url| based on classifying |text|.
  // |alternate_nav_url| may be NULL.
  void ClassifyString(const base::string16& text,
                      AutocompleteMatch* match,
                      GURL* alternate_nav_url) const;

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

  // NOTE: |client_| must outlive |omnibox_controller_|, as the latter has a
  // reference to the former.
  std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<OmniboxController> omnibox_controller_;

  OmniboxView* view_;

  OmniboxEditController* controller_;

  OmniboxFocusState focus_state_;

  // Used to keep track whether the input currently in progress originated by
  // focusing in the Omnibox, Fakebox or Search button. This will be INVALID if
  // no input is in progress or the Omnibox is not focused.
  FocusSource focus_source_;

  // Display-only text representing the current page. This could be any of:
  //  - The same as |url_for_editing_| if Steady State Elisions is OFF.
  //  - A simplified version of |url_for_editing_| with some destructive
  //    elisions applied. This is the case if Steady State Elisions is ON.
  //  - The user entered search query, if the user is on the search results
  //    page of the default search provider and Query in Omnibox is ON.
  //
  // This should not be considered suitable for editing.
  base::string16 display_text_;

  // The initial text representing the current URL suitable for editing.
  // This should fully represent the current URL without any meaning-changing
  // elisions applied - and is suitable for user editing.
  base::string16 url_for_editing_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag will be true in a superset of the cases where the popup is open.
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.
  base::string16 user_text_;

  // We keep track of when the user last focused on the omnibox.
  base::TimeTicks last_omnibox_focus_;

  // Whether any user input has occurred since focusing on the omnibox. This is
  // used along with |last_omnibox_focus_| to calculate the time between a user
  // focusing on the omnibox and editing. It is initialized to true since
  // there was no focus event.
  bool user_input_since_focus_;

  // We keep track of when the user began modifying the omnibox text.
  // This should be valid whenever user_input_in_progress_ is true.
  base::TimeTicks time_user_first_modified_omnibox_;

  // When the user closes the popup, we need to remember the URL for their
  // desired choice, so that if they hit enter without reopening the popup we
  // know where to go.  We could simply rerun autocomplete in this case, but
  // we'd need to either wait for all results to come in (unacceptably slow) or
  // do the wrong thing when the user had chosen some provider whose results
  // were not returned instantaneously.
  //
  // This variable is only valid when user_input_in_progress_ is true, since
  // when it is false the user has either never input anything (so there won't
  // be a value here anyway) or has canceled their input, which should be
  // treated the same way.  Also, since this is for preserving a desired URL
  // after the popup has been closed, we ignore this if the popup is open, and
  // simply ask the popup for the desired URL directly.  As a result, the
  // contents of this variable only need to be updated when the popup is closed
  // but user_input_in_progress_ is not being cleared.
  base::string16 url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocomplete_text_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  base::string16 inline_autocomplete_text_;

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
  // original_url_ is only valid when there is temporary text, and is used as
  // the unique identifier of the originally selected item.  Thus, if the user
  // arrows to a different item with the same text, we can still distinguish
  // them and not revert all the way to the permanent display URL.
  bool has_temporary_text_;
  GURL original_url_;

  // When the user's last action was to paste, we disallow inline autocomplete
  // (on the theory that the user is trying to paste in a new URL or part of
  // one, and in either case inline autocomplete would get in the way).
  PasteState paste_state_;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_;

  // The keyword associated with the current match.  The user may have an actual
  // selected keyword, or just some input text that looks like a keyword (so we
  // can show a hint to press <tab>).  This is the keyword in either case;
  // is_keyword_hint_ (below) distinguishes the two cases.
  base::string16 keyword_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_;

  // Indicates how the user entered keyword mode if the user is actually in
  // keyword mode.  Otherwise, the value of this variable is undefined.  This
  // is used to restore the user's search terms upon a call to ClearKeyword().
  KeywordModeEntryMethod keyword_mode_entry_method_;

  // This is needed to properly update the SearchModel state when the user
  // presses escape.
  bool in_revert_;

  // Indicates if the upcoming autocomplete search is allowed to be treated as
  // an exact keyword match.  If this is true then keyword mode will be
  // triggered automatically if the input is "<keyword> <search string>".  We
  // allow this when CreatedKeywordSearchByInsertingSpaceInMiddle() is true.
  // This has no effect if we're already in keyword mode.
  bool allow_exact_keyword_match_;

  // The input that was sent to the AutocompleteController. Since no
  // autocomplete query is started after a tab switch, it is possible for this
  // |input_| to differ from the one currently stored in AutocompleteController.
  AutocompleteInput input_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxEditModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_
