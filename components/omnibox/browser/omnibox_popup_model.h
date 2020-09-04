// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_

#include <stddef.h>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

class OmniboxPopupModelObserver;
class OmniboxPopupView;
class GURL;
class PrefService;

namespace gfx {
class Image;
}

class OmniboxPopupModel {
 public:
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

  // The sentinel value for Selection::line which means no line is selected.
  static const size_t kNoMatch;

  // See |Selection::state| below for details. The numeric values are to aid
  // comparison only. They are not persisted anywhere and can be freely changed.
  enum LineState {
    // This means the Header above this row is highlighted, and the
    // header collapse/expand button is focused.
    FOCUSED_BUTTON_HEADER = 0,

    // NORMAL means the row is focused, and Enter key navigates to the match.
    NORMAL = 1,

    // KEYWORD_MODE state is used when in Keyword mode.  If the keyword search
    // button is enabled, keyword mode is entered when the keyword button is
    // focused.
    KEYWORD_MODE = 2,

    // FOCUSED_BUTTON_TAB_SWITCH state means the Switch Tab button is focused.
    // Pressing enter will switch to the tab match.
    FOCUSED_BUTTON_TAB_SWITCH = 3,

    // FOCUSED_BUTTON_PEDAL state means a Pedal button is in focus. This is
    // currently only used when dedicated button row and pedals are enabled.
    FOCUSED_BUTTON_PEDAL = 4,

    // FOCUSED_BUTTON_REMOVE_SUGGESTION state means the Remove Suggestion (X)
    // button is focused. Pressing enter will attempt to remove this suggestion.
    FOCUSED_BUTTON_REMOVE_SUGGESTION = 5,

    // Whenever new line state is added, accessibility label for current
    // selection should be revisited
    // (OmniboxPopupModel::GetAccessibilityLabelForCurrentSelection).
    LINE_STATE_MAX_VALUE
  };

  struct Selection {
    // The currently selected line.  This is kNoMatch when nothing is selected,
    // which should only be true when the popup is closed.
    size_t line;

    // If the selected line has both a normal match and a keyword match, this
    // determines whether the normal match (if NORMAL) or the keyword match
    // (if KEYWORD) is selected. Likewise, if the selected line has a normal
    // match and a secondary button match, this determines whether the button
    // match (if FOCUSED_BUTTON_*) is selected.
    LineState state;

    explicit Selection(size_t line, LineState state = NORMAL)
        : line(line), state(state) {}

    bool operator==(const Selection&) const;
    bool operator!=(const Selection&) const;
    bool operator<(const Selection&) const;

    // Returns true if going to this selection from given |from| selection
    // results in activation of keyword state when it wasn't active before.
    bool IsChangeToKeyword(Selection from) const;

    // Returns true if this selection represents a button being focused.
    bool IsButtonFocused() const;
  };

  // |pref_service| can be nullptr, in which case OmniboxPopupModel won't
  // account for collapsed headers.
  OmniboxPopupModel(OmniboxPopupView* popup_view,
                    OmniboxEditModel* edit_model,
                    PrefService* pref_service);
  ~OmniboxPopupModel();
  OmniboxPopupModel(const OmniboxPopupModel&) = delete;
  OmniboxPopupModel& operator=(const OmniboxPopupModel&) = delete;

  // Computes the maximum width, in pixels, that can be allocated for the two
  // parts of an autocomplete result, i.e. the contents and the description.
  //
  // When |description_on_separate_line| is true, the caller will be displaying
  // two separate lines of text, so both contents and description can take up
  // the full available width. Otherwise, the contents and description are
  // assumed to be on the same line, with a separator between them.
  //
  // When |allow_shrinking_contents| is true, and the contents and description
  // are together on a line without enough space for both, the code tries to
  // divide the available space equally between the two, unless this would make
  // one or both too narrow. Otherwise, the contents is given as much space as
  // it wants and the description gets the remainder.
  static void ComputeMatchMaxWidths(int contents_width,
                                    int separator_width,
                                    int description_width,
                                    int available_width,
                                    bool description_on_separate_line,
                                    bool allow_shrinking_contents,
                                    int* contents_max_width,
                                    int* description_max_width);

  // Returns true if the popup is currently open.
  bool IsOpen() const;

  OmniboxPopupView* view() const { return view_; }
  OmniboxEditModel* edit_model() const { return edit_model_; }

  // Returns the AutocompleteController used by this popup.
  AutocompleteController* autocomplete_controller() const {
    return edit_model_->autocomplete_controller();
  }

  const AutocompleteResult& result() const {
    return autocomplete_controller()->result();
  }

  Selection selection() const { return selection_; }
  size_t selected_line() const { return selection_.line; }
  LineState selected_line_state() const { return selection_.state; }

  // Sets the current selection to |new_selection|. Caller is responsible for
  // making sure |new_selection| is valid. This assumes the popup is open.
  //
  // This will update all state and repaint the necessary parts of the window,
  // as well as updating the textfield with the new temporary text.
  //
  // |reset_to_default| restores the original inline autocompletion.
  // |force_update_ui| updates the UI even if the selection has not changed.
  void SetSelection(Selection new_selection,
                    bool reset_to_default = false,
                    bool force_update_ui = false);

  // We still need to run through the whole SetSelection method, because
  // changing the line state sometimes requires updating inline autocomplete.
  void SetSelectedLineState(LineState new_state) {
    SetSelection(Selection(selected_line(), new_state));
  }

  // Called when the user hits escape after arrowing around the popup.  This
  // will reset the popup to the initial state.
  void ResetToInitialState();

  // Tries to erase the suggestion at |line|.  This should determine if the item
  // at |line| can be removed from history, and if so, remove it and update the
  // popup.
  void TryDeletingLine(size_t line);

  // Returns true if the destination URL of the match is bookmarked.
  bool IsStarredMatch(const AutocompleteMatch& match) const;

  // Returns true if the selection is on the initial line, which is usually the
  // default match (except in the no-default-match case).
  bool SelectionOnInitialLine() const;

  // Invoked from the edit model any time the result set of the controller
  // changes.
  void OnResultChanged();

  // Add and remove observers.
  void AddObserver(OmniboxPopupModelObserver* observer);
  void RemoveObserver(OmniboxPopupModelObserver* observer);

  // Lookup the bitmap for |result_index|. Returns nullptr if not found.
  const SkBitmap* RichSuggestionBitmapAt(int result_index) const;
  // Stores the image in a local data member and schedules a repaint.
  void SetRichSuggestionBitmap(int result_index, const SkBitmap& bitmap);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Gets the icon for the match index.
  gfx::Image GetMatchIcon(const AutocompleteMatch& match,
                          SkColor vector_icon_color);
#endif

  OmniboxEditModel* edit_model() { return edit_model_; }

  // Gets all the available selections, filtered by |direction| and |step|, as
  // well as feature flags and the matches' states.
  std::vector<Selection> GetAllAvailableSelectionsSorted(Direction direction,
                                                         Step step) const;

  // Advances selection with consideration for both line number and line state.
  // |direction| indicates direction of step, and |step| determines what kind
  // of step to take. Returns the next selection, which could be anything.
  Selection GetNextSelection(Direction direction, Step step) const;

  // Applies the next selection as provided by GetNextSelection.
  // Stepping the popup model selection gives special consideration for
  // keyword mode state maintained in the edit model.
  Selection StepSelection(Direction direction, Step step);

  // Returns true if the control represented by |selection.state| is present on
  // the match in |selection.line|. This is the source-of-truth the UI code
  // should query to decide whether or not to draw the control.
  bool IsControlPresentOnMatch(Selection selection) const;

  // Triggers the action on |selection| (usually an auxiliary button).
  // If the popup model supports the action and performs it, this returns true.
  // This can't handle all actions currently, and returns false in those cases.
  // The timestamp parameter is currently only used by FOCUSED_BUTTON_TAB_SWITCH
  // and FOCUSED_BUTTON_PEDAL, so is set by default for other use cases.
  bool TriggerSelectionAction(Selection selection,
                              base::TimeTicks timestamp = base::TimeTicks());

  // This returns the accessibility label for current selection. This is an
  // extended version of AutocompleteMatchType::ToAccessibilityLabel() which
  // also returns narration about the any focused secondary button.
  // Never call this when the current selection is kNoMatch.
  base::string16 GetAccessibilityLabelForCurrentSelection(
      const base::string16& match_text,
      bool include_positional_info,
      int* label_prefix_length = nullptr);

 private:
  void OnFaviconFetched(const GURL& page_url, const gfx::Image& icon);

  std::map<int, SkBitmap> rich_suggestion_bitmaps_;

  OmniboxPopupView* view_;

  OmniboxEditModel* edit_model_;

  // Non-owning reference to the pref service. Can be nullptr in tests or iOS.
  PrefService* const pref_service_;

  Selection selection_;

  // When a result changes, this informs of the URL in the previously selected
  // suggestion whose tab switch button was focused, so that we may compare
  // if equal.
  GURL old_focused_url_;

  // Observers.
  base::ObserverList<OmniboxPopupModelObserver>::Unchecked observers_;

  base::WeakPtrFactory<OmniboxPopupModel> weak_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_
