// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxEditModel;
class OmniboxPopupViewViews;
class OmniboxSuggestionRowButton;

namespace views {
class Button;
}

// A view to contain the button row within a result view.
class OmniboxSuggestionButtonRowView : public views::View {
 public:
  METADATA_HEADER(OmniboxSuggestionButtonRowView);
  explicit OmniboxSuggestionButtonRowView(OmniboxPopupViewViews* view,
                                          OmniboxEditModel* model,
                                          int model_index);
  OmniboxSuggestionButtonRowView(const OmniboxSuggestionButtonRowView&) =
      delete;
  OmniboxSuggestionButtonRowView& operator=(
      const OmniboxSuggestionButtonRowView&) = delete;
  ~OmniboxSuggestionButtonRowView() override;

  // Called when the theme state may have changed.
  void SetThemeState(OmniboxPartState theme_state);

  // Updates the suggestion row buttons based on the model.
  void UpdateFromModel();

  // Called when the selected item (row or button) in the popup has changed.
  void SelectionStateChanged();

  views::Button* GetActiveButton() const;

 private:
  // Digs into the model with index to get the match for owning result view.
  const AutocompleteMatch& match() const;

  void SetPillButtonVisibility(OmniboxSuggestionRowButton* button,
                               OmniboxPopupSelection::LineState state);

  void ButtonPressed(OmniboxPopupSelection::LineState state,
                     const ui::Event& event);

  const raw_ptr<OmniboxPopupViewViews> popup_contents_view_;
  raw_ptr<OmniboxEditModel> model_;
  size_t const model_index_;

  raw_ptr<OmniboxSuggestionRowButton> keyword_button_ = nullptr;
  // TODO(manukh): Rename `pedal_button_` to `action_button_` as it is shared by
  //  other actions ('journeys' currently).
  raw_ptr<OmniboxSuggestionRowButton> pedal_button_ = nullptr;
  raw_ptr<OmniboxSuggestionRowButton> tab_switch_button_ = nullptr;

  // Which button, if any, was active as of the last call to
  // SelectionStateChanged().
  raw_ptr<views::Button> previous_active_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
