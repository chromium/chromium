// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_SELECTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_SELECTION_H_

#include <stddef.h>

#include "components/prefs/pref_service.h"

class AutocompleteResult;
class TemplateURLService;

struct OmniboxPopupSelection {
  // Directions for stepping through selections. These may apply for going
  // up/down by lines or cycling left/right through states within a line.
  enum Direction { kForward, kBackward };

  // When changing selections, these are the possible stepping behaviors.
  enum Step {
    // Step by an entire line regardless of line state. Usually used for the
    // Up and Down arrow keys.
    kWholeLine,

    // Step by a state if another one is available on the current line;
    // otherwise step by line. Usually used for the Tab and Shift+Tab keys.
    kStateOrLine,

    // Step across all lines to the first or last line. Usually used for the
    // PgUp and PgDn keys.
    kAllLines
  };

  // See `state` below for details. The order matters; earlier items will be
  // selected first when tabbing through the popup. They are not persisted
  // anywhere and can be freely changed.
  enum LineState {
    // This means the Header above this row is highlighted, and the
    // header collapse/expand button is focused.
    FOCUSED_BUTTON_HEADER,

    // NORMAL means the row is focused, and Enter key navigates to the match.
    NORMAL,

    // KEYWORD_MODE state is used when in Keyword mode.  If the keyword search
    // button is enabled, keyword mode is entered when the keyword button is
    // focused.
    KEYWORD_MODE,

    // FOCUSED_BUTTON_ACTION state means an Action button (such as a Pedal)
    // is in focus.
    FOCUSED_BUTTON_ACTION,

    // FOCUSED_BUTTON_THUMBS_UP state means the thumbs up button is focused.
    FOCUSED_BUTTON_THUMBS_UP,

    // FOCUSED_BUTTON_THUMBS_DOWN state means the thumbs down button is focused.
    // Pressing enter will attempt to submit feedback form for this suggestion.
    FOCUSED_BUTTON_THUMBS_DOWN,

    // FOCUSED_BUTTON_REMOVE_SUGGESTION state means the Remove Suggestion (X)
    // button is focused. Pressing enter will attempt to remove this suggestion.
    FOCUSED_BUTTON_REMOVE_SUGGESTION,

    // Whenever new line state is added, accessibility label for current
    // selection should be revisited
    // (`OmniboxEditModel::GetPopupAccessibilityLabelForCurrentSelection()`).
    LINE_STATE_MAX_VALUE
  };

  // The sentinel value for `line` which means no line is selected.
  static const size_t kNoMatch;

  // The selected line. This is `kNoMatch` when nothing is selected, which
  // should only be true when a) the popup is closed or b) an empty suggestion
  // is selected (e.g. the default suggestion in zero-input mode).
  size_t line;

  // If the selected line has both a normal match and a keyword match, this
  // determines whether the normal match (if NORMAL) or the keyword match
  // (if KEYWORD) is selected. Likewise, if the selected line has a normal
  // match and a secondary button match, this determines whether the button
  // match (if FOCUSED_BUTTON_*) is selected.
  LineState state;

  // When `state` is `FOCUSED_BUTTON_ACTION`, this indicates which action
  // is selected by index into `AutocompleteMatch::actions`. Other states
  // keep an unused zero index.
  size_t action_index;

  explicit OmniboxPopupSelection(size_t line,
                                 LineState state = NORMAL,
                                 size_t action_index = 0)
      : line(line), state(state), action_index(action_index) {}

  bool operator==(const OmniboxPopupSelection&) const;
  bool operator!=(const OmniboxPopupSelection&) const;
  bool operator<(const OmniboxPopupSelection&) const;

  // Returns true if going to this selection from given `from` selection
  // results in activation of keyword state when it wasn't active before.
  bool IsChangeToKeyword(OmniboxPopupSelection from) const;

  // Returns true if this selection represents a button being focused.
  bool IsButtonFocused() const;

  // Returns true if this selection represents taking an action.
  bool IsAction() const;

  // Returns true if the control represented by this selection's `state` is
  // present on the match for `line` in given `result`.
  bool IsControlPresentOnMatch(const AutocompleteResult& result,
                               const PrefService* pref_service) const;

  // Returns the next selection after this one in given `result`.
  OmniboxPopupSelection GetNextSelection(
      const AutocompleteResult& result,
      const PrefService* pref_service,
      TemplateURLService* template_url_service,
      Direction direction,
      Step step) const;

 private:
  //  This is a utility function to support `GetNextSelection`.
  static std::vector<OmniboxPopupSelection> GetAllAvailableSelectionsSorted(
      const AutocompleteResult& result,
      const PrefService* pref_service,
      TemplateURLService* template_url_service,
      Direction direction,
      Step step);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_SELECTION_H_
