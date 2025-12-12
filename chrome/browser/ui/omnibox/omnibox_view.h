// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxView.  Each toolkit will
// implement the edit view differently, so that code is inherently platform
// specific.  However, the OmniboxEditModel needs to do some communication with
// the view.  Since the model is shared between platforms, we need to define an
// interface that all view implementations will share.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

class OmniboxController;
class OmniboxEditModel;

class OmniboxView {
  // TODO(crbug.com/392015004): Remove this macro once it gets fixed.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  using IconFetchedCallback = base::OnceCallback<void(const gfx::Image& icon)>;

  // Represents the changes between two State objects.  This is used by the
  // model to determine how its internal state should be updated after the view
  // state changes.  See OmniboxEditModel::OnAfterPossibleChange().
  struct StateChanges {
    // |old_text| and |new_text| are not owned.
    raw_ptr<const std::u16string> old_text;
    raw_ptr<const std::u16string> new_text;
    gfx::Range new_selection;
    bool selection_differs;
    bool text_differs;
    bool keyword_differs;
    bool just_deleted_text;
  };

  OmniboxView(const OmniboxView&) = delete;
  OmniboxView& operator=(const OmniboxView&) = delete;
  virtual ~OmniboxView();

  // Called when any relevant state changes other than changing tabs.
  virtual void Update() = 0;

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::u16string GetText() const = 0;

  // |true| if the user is in the process of editing the field, or if
  // the field is empty.
  bool IsEditingOrEmpty() const;

#if !BUILDFLAG(IS_ANDROID)
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
#endif

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

  // Returns the indexes of the current selection's bounds. Note that the
  // selection can be directed, so the result may be a reverse range.
  // If there is no selection, the range's values will both be equal to the
  // current cursor position.
  virtual gfx::Range GetSelectionBounds() const = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll(bool reversed) = 0;

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup() = 0;

  // Sets the focus to the omnibox. |is_user_initiated| is true when the user
  // explicitly focused the omnibox, and false when the omnibox was
  // automatically focused (like for browser startup or NTP load).
  virtual void SetFocus(bool is_user_initiated) = 0;

  // Applies a focus ring predicate to control when the AIM button's focus ring
  // is shown. If `force_focus` is true, the focus ring will always be shown.
  // This is used to indicated focus when the popup selection selects the AIM
  // button, even though the omnibox is still the focused view.  If
  // `force_focus` is false, the focus ring will use the standard behavior,
  // which is to show the focus ring when the button has focus.
  virtual void ApplyFocusRingToAimButton(bool force_focus) {}

  // Returns true if the AI mode entrypoint button is visible.
  virtual bool AimButtonVisible() const = 0;

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
  // `user_text` is the portion of omnibox text the user typed.
  // `inline`_autocompletion` is the autocompleted part.
  virtual void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
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

  // Returns true if we know for sure that an IME is showing a popup window,
  // which may overlap the omnibox's popup window.
  virtual bool IsImeShowingPopup() const;

  // Display a virtual keyboard or alternate input view if enabled.
  virtual void ShowVirtualKeyboardIfEnabled();

  // Hides a virtual keyboard or alternate input view if enabled.
  virtual void HideImeIfNeeded();

 protected:
  // Tracks important state that may change between OnBeforePossibleChange() and
  // OnAfterPossibleChange().
  struct State {
    std::u16string text;
    std::u16string keyword;
    bool is_keyword_selected;
    gfx::Range selection;
  };

  explicit OmniboxView(OmniboxController* controller);

  // Returns the current text state.
  State GetState() const;

  // Returns the delta between |before| and |after|.
  static StateChanges GetStateChanges(const State& before, const State& after);

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

  virtual OmniboxController* controller();
  virtual const OmniboxController* controller() const;

 private:
  // Owned by the LocationBarView that owns this. Outlives this.
  raw_ptr<OmniboxController> controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_H_
