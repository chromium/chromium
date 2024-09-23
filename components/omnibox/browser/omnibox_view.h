// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxView.  Each toolkit will
// implement the edit view differently, so that code is inherently platform
// specific.  However, the OmniboxEditModel needs to do some communication with
// the view.  Since the model is shared between platforms, we need to define an
// interface that all view implementations will share.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

class OmniboxController;
class OmniboxEditModel;
class OmniboxViewMacTest;

class OmniboxView {
 public:
  using IconFetchedCallback = base::OnceCallback<void(const gfx::Image& icon)>;

  // Represents the changes between two State objects.  This is used by the
  // model to determine how its internal state should be updated after the view
  // state changes.  See OmniboxEditModel::OnAfterPossibleChange().
  struct StateChanges {
    // |old_text| and |new_text| are not owned.
    raw_ptr<const std::u16string> old_text;
    raw_ptr<const std::u16string> new_text;
    size_t new_sel_start;
    size_t new_sel_end;
    bool selection_differs;
    bool text_differs;
    bool keyword_differs;
    bool just_deleted_text;
  };

  virtual ~OmniboxView();
  OmniboxView(const OmniboxView&) = delete;
  OmniboxView& operator=(const OmniboxView&) = delete;

  OmniboxEditModel* model();
  const OmniboxEditModel* model() const;

  OmniboxController* controller();
  const OmniboxController* controller() const;

  // Called when any relevant state changes other than changing tabs.
  virtual void Update() = 0;

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::u16string GetText() const = 0;

  // |true| if the user is in the process of editing the field, or if
  // the field is empty.
  bool IsEditingOrEmpty() const;

  // Returns the icon to display as the location icon. If a favicon is
  // available, `on_icon_fetched` may be called later asynchronously.
  // `color_current_page_icon` is used for the page icon (i.e. when the popup is
  // closed, there is no input in progress, and there's a URL displayed) (e.g.
  // the secure page lock). `color_vectors` is used for vector icons e.g. the
  // history clock or bookmark star. `color_bright_vectors` is used for special
  // vector icons e.g. the history cluster squiggle.
  // `color_vectors_with_background` is used for vector icons that are drawn
  // atop a background e.g. action suggestions. Favicons aren't custom-colored.
  // `dark_mode` returns the dark_mode version of an icon. This should usually
  // be handled by `color_current_page_icon` but in cases where the icon has
  // hardcoded colors this can be used to return a different icon. E.g., the
  // SuperGIcon will return different icons in dark and light modes.
  ui::ImageModel GetIcon(int dip_size,
                         SkColor color_current_page_icon,
                         SkColor color_vectors,
                         SkColor color_bright_vectors,
                         SkColor color_vectors_with_background,
                         IconFetchedCallback on_icon_fetched,
                         bool dark_mode) const;

  // The user text is the text the user has manually keyed in.  When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const std::u16string& text);
  virtual void SetUserText(const std::u16string& text, bool update_popup);

  // Sets the window text and the caret position. |notify_text_changed| is true
  // if the model should be notified of the change. Clears the additional text.
  virtual void SetWindowTextAndCaretPos(const std::u16string& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) = 0;

  // Sets the caret position. Removes any selection. Clamps the requested caret
  // position to the length of the current text.
  virtual void SetCaretPos(size_t caret_pos) = 0;

  // Sets the omnibox adjacent additional text label in the location bar view.
  virtual void SetAdditionalText(const std::u16string& text) = 0;

  // Transitions the user into keyword mode with their default search provider,
  // preserving and selecting the user's text if they already typed in a query.
  virtual void EnterKeywordModeForDefaultSearchProvider() = 0;

  // Returns true if all text is selected. Returns false if there is no text.
  virtual bool IsSelectAll() const = 0;

  // Fills |start| and |end| with the indexes of the current selection's bounds.
  // It is not guaranteed that |*start < *end|, as the selection can be
  // directed.  If there is no selection, |start| and |end| will both be equal
  // to the current cursor position.
  virtual void GetSelectionBounds(size_t* start, size_t* end) const = 0;

  // Returns the sum of all selections' lengths. This is used to detect when
  // the user has deleted text, and therefore, their input should not be
  // autocompleted.
  virtual size_t GetAllSelectionsLength() const = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll(bool reversed) = 0;

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup() = 0;

  // Closes the autocomplete popup, if it's open. The name |ClosePopup|
  // conflicts with the OSX class override as that has a base class that also
  // defines a method with that name.
  virtual void CloseOmniboxPopup();

  // Sets the focus to the omnibox. |is_user_initiated| is true when the user
  // explicitly focused the omnibox, and false when the omnibox was
  // automatically focused (like for browser startup or NTP load).
  virtual void SetFocus(bool is_user_initiated) = 0;

  // Shows or hides the caret based on whether the model's is_caret_visible() is
  // true.
  virtual void ApplyCaretVisibility() = 0;

  // Updates the accessibility state by enunciating any on-focus text.
  virtual void SetAccessibilityLabel(const std::u16string& display_text,
                                     const AutocompleteMatch& match,
                                     bool notify_text_changed) {}

  // Called when the temporary text in the model may have changed.
  // |display_text| is the new text to show; |match_type| is the type of the
  // match the new text came from. |save_original_selection| is true when there
  // wasn't previously a temporary text and thus we need to save off the user's
  // existing selection. |notify_text_changed| is true if the model should be
  // notified of the change.
  virtual void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                           const AutocompleteMatch& match,
                                           bool save_original_selection,
                                           bool notify_text_changed) = 0;

  // Called when the inline autocomplete text in the model may have changed.
  // `display_text` is the new text to show. `selection` indicates the
  // autocompleted portions which should be selected. `*_autocompletion`
  // represents the appropriate autocompletion.
  // TODO(manukh) The last 3 parameters are redundant except when split
  //  autocompletion is enabled. Once we've cleaned up split autocompletion
  //  (it's unlikely to launch but still useful for experimenting), `selections`
  //  should be removed.
  virtual void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& display_text,
      std::vector<gfx::Range> selections,
      const std::u16string& prefix_autocompletion,
      const std::u16string& inline_autocompletion) = 0;

  // Called when the inline autocomplete text in the model has been cleared.
  virtual void OnInlineAutocompleteTextCleared() = 0;

  // Called when the temporary text has been reverted by the user.  This will
  // reset the user's original selection.
  virtual void OnRevertTemporaryText(const std::u16string& display_text,
                                     const AutocompleteMatch& match) = 0;

  // Checkpoints the current edit state before an operation that might trigger
  // a new autocomplete run to open or modify the popup. Call this before
  // user-initiated edit actions that trigger autocomplete, but *not* for
  // automatic changes to the textfield that should not affect autocomplete.
  virtual void OnBeforePossibleChange() = 0;

  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().  If |allow_keyword_ui_change| is false, we
  // prevent alterations to the keyword UI state (enabled vs. disabled).
  virtual bool OnAfterPossibleChange(bool allow_keyword_ui_change) = 0;

  // Called when the placeholder text displayed when the user is in keyword mode
  // has changed.
  virtual void OnKeywordPlaceholderTextChange() {}

  // Returns the gfx::NativeView of the edit view.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Gets the relative window for the pop up window of OmniboxPopupView. The pop
  // up window will be shown under the relative window. When an IME is attached
  // to the rich edit control, the IME window is the relative window. Otherwise,
  // the top-most window is the relative window.
  virtual gfx::NativeView GetRelativeWindowForPopup() const = 0;

  // Returns true if the user is composing something in an IME.
  virtual bool IsImeComposing() const = 0;

  // Returns true if we know for sure that an IME is showing a popup window,
  // which may overlap the omnibox's popup window.
  virtual bool IsImeShowingPopup() const;

  // Display a virtual keyboard or alternate input view if enabled.
  virtual void ShowVirtualKeyboardIfEnabled();

  // Hides a virtual keyboard or alternate input view if enabled.
  virtual void HideImeIfNeeded();

  // Returns true if the view is displaying UI that indicates that query
  // refinement will take place when the user selects the current match.  For
  // search matches, this will cause the omnibox to search over the existing
  // corpus (e.g. Images) rather than start a new Web search.  This method will
  // only ever return true on mobile ports.
  virtual bool IsIndicatingQueryRefinement() const;

  // Returns |text| with any leading javascript schemas stripped.
  static std::u16string StripJavascriptSchemas(const std::u16string& text);

  // Automatically collapses internal whitespace as follows:
  // * Leading and trailing whitespace are often copied accidentally and rarely
  //   affect behavior, so they are stripped.  If this collapses the whole
  //   string, returns a space, since pasting nothing feels broken.
  // * Internal whitespace sequences not containing CR/LF may be integral to the
  //   meaning of the string and are preserved exactly.  The presence of any of
  //   these also suggests the input is more likely a search than a navigation,
  //   which affects the next bullet.
  // * Internal whitespace sequences containing CR/LF have likely been split
  //   across lines by terminals, email programs, etc., and are collapsed.  If
  //   there are any internal non-CR/LF whitespace sequences, the input is more
  //   likely search data (e.g. street addresses), so collapse these to a single
  //   space.  If not, the input might be a navigation (e.g. a line-broken URL),
  //   so collapse these away entirely.
  //
  // Finally, calls StripJavascriptSchemas() on the resulting string.
  static std::u16string SanitizeTextForPaste(const std::u16string& text);

 protected:
  // Tracks important state that may change between OnBeforePossibleChange() and
  // OnAfterPossibleChange().
  struct State {
    std::u16string text;
    std::u16string keyword;
    bool is_keyword_selected;
    size_t sel_start;
    size_t sel_end;
    size_t all_sel_length;

    State();
    State(const State& state);
  };

  explicit OmniboxView(std::unique_ptr<OmniboxClient> client);

  // Fills |state| with the current text state.
  void GetState(State* state);

  // Returns the delta between |before| and |after|.
  StateChanges GetStateChanges(const State& before, const State& after);

  // Internally invoked whenever the text changes in some way.
  virtual void TextChanged();

  // Return the number of characters in the current buffer. The name
  // |GetTextLength| can't be used as the Windows override of this class
  // inherits from a class that defines a method with that name.
  virtual int GetOmniboxTextLength() const = 0;

  // Try to parse the current text as a URL and colorize the components.
  virtual void EmphasizeURLComponents() = 0;

  // Marks part (or, if |range| is invalid, all) of the current text as
  // emphasized or de-emphasized, by changing its color.
  virtual void SetEmphasis(bool emphasize, const gfx::Range& range) = 0;

  // Sets the color and strikethrough state for |range|, which represents the
  // current scheme, based on the current security state.  Schemes are displayed
  // in different ways for different security levels.
  virtual void UpdateSchemeStyle(const gfx::Range& range) = 0;

  // Parses |display_text|, then invokes SetEmphasis() and UpdateSchemeStyle()
  // appropriately. If the text is a query string, there is no scheme, and
  // everything is emphasized equally, whereas for URLs the scheme may be styled
  // based on the current security state, with parts of the URL de-emphasized to
  // draw attention to whatever best represents the "identity" of the current
  // URL.
  void UpdateTextStyle(const std::u16string& display_text,
                       const bool text_is_url,
                       const AutocompleteSchemeClassifier& classifier);

 private:
  friend class OmniboxViewMacTest;
  friend class TestOmniboxView;

  std::unique_ptr<OmniboxController> controller_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_
