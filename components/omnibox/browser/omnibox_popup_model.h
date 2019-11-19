// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_

#include <stddef.h>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

class OmniboxPopupModelObserver;
class OmniboxPopupView;
class GURL;

namespace gfx {
class Image;
}

class OmniboxPopupModel {
 public:
  // See selected_line_state_ for details.
  enum LineState {
    NORMAL = 0,
    KEYWORD,
    BUTTON_FOCUSED
  };

  OmniboxPopupModel(OmniboxPopupView* popup_view, OmniboxEditModel* edit_model);
  ~OmniboxPopupModel();

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

  size_t selected_line() const { return selected_line_; }

  LineState selected_line_state() const { return selected_line_state_; }

  // Call to change the selected line.  This will update all state and repaint
  // the necessary parts of the window, as well as updating the edit with the
  // new temporary text.  |line| will be clamped to the range of valid lines.
  // |reset_to_default| is true when the selection is being reset back to the
  // initial state, and thus there is no temporary text (and not
  // |has_selected_match_|). If |force| is true then the selected line will
  // be updated forcibly even if the |line| is same as the current selected
  // line.
  // NOTE: This assumes the popup is open, although both the old and new values
  // for the selected line can be kNoMatch.
  void SetSelectedLine(size_t line, bool reset_to_default, bool force);

  // Called when the user hits escape after arrowing around the popup.  This
  // will reset the popup to the initial state.
  void ResetToInitialState();

  // Immediately updates and opens the popup if necessary, then moves the
  // current selection to the respective line. If the line is unchanged, the
  // selection will be unchanged, but the popup will still redraw and modify
  // the text in the OmniboxEditModel.
  void MoveTo(size_t new_line);

  // If the selected line has both a normal match and a keyword match, this can
  // be used to choose which to select.  This allows the user to toggle between
  // normal and keyword mode with tab/shift-tab without rerunning autocomplete
  // or disturbing other popup state, which in turn is an important part of
  // supporting the use of tab to do both tab-to-search and
  // tab-to-traverse-dropdown.
  //
  // It is an error to call this when the selected line does not have both
  // matches (or there is no selection).
  void SetSelectedLineState(LineState state);

  // Tries to erase the suggestion at |line|.  This should determine if the item
  // at |line| can be removed from history, and if so, remove it and update the
  // popup.
  void TryDeletingLine(size_t line);

  // Returns true if the destination URL of the match is bookmarked.
  bool IsStarredMatch(const AutocompleteMatch& match) const;

  // The user has manually selected a match.
  bool has_selected_match() { return has_selected_match_; }

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

  // Helper function to see if the current selection specifically has a
  // tab switch button.
  bool SelectedLineHasTabMatch();

  // Helper function to see if current selection is a tab switch suggestion
  // dedicated row.
  bool SelectedLineIsTabSwitchSuggestion();

  // If |closes| is set true, the popup will close when the omnibox is blurred.
  bool popup_closes_on_blur() const { return popup_closes_on_blur_; }
  void set_popup_closes_on_blur(bool closes) { popup_closes_on_blur_ = closes; }

  OmniboxEditModel* edit_model() { return edit_model_; }

  // The token value for selected_line_ and functions dealing with a "line
  // number" that indicates "no line".
  static const size_t kNoMatch;

 private:
  void OnFaviconFetched(const GURL& page_url, const gfx::Image& icon);

  std::map<int, SkBitmap> rich_suggestion_bitmaps_;

  OmniboxPopupView* view_;

  OmniboxEditModel* edit_model_;

  // The currently selected line.  This is kNoMatch when nothing is selected,
  // which should only be true when the popup is closed.
  size_t selected_line_;

  // If the selected line has both a normal match and a keyword match, this
  // determines whether the normal match (if NORMAL) or the keyword match
  // (if KEYWORD) is selected. Likewise, if the selected line has a normal
  // match and a tab switch match, this determines whether the tab switch match
  // (if TAB_SWITCH) is selected.
  LineState selected_line_state_;

  // When a result changes, this informs of the URL in the previously selected
  // suggestion whose tab switch button was focused, so that we may compare
  // if equal.
  GURL old_focused_url_;

  // The user has manually selected a match.
  // TODO(tommycli): We can _probably_ eliminate this variable. It seems to be
  // mostly rendundant with selected_line() and result()->default_match().
  bool has_selected_match_;

  // True if the popup should close on omnibox blur. This defaults to true, and
  // is only false while a bubble related to the popup contents is shown.
  bool popup_closes_on_blur_ = true;

  // Observers.
  base::ObserverList<OmniboxPopupModelObserver>::Unchecked observers_;

  base::WeakPtrFactory<OmniboxPopupModel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_MODEL_H_
