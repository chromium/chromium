// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class OmniboxPopupContentsView;
class OmniboxSuggestionRowButton;

namespace views {
class Button;
}

// A view to contain the button row within a result view.
class OmniboxSuggestionButtonRowView : public views::View {
 public:
  METADATA_HEADER(OmniboxSuggestionButtonRowView);
  explicit OmniboxSuggestionButtonRowView(OmniboxPopupContentsView* view,
                                          int model_index);
  OmniboxSuggestionButtonRowView(const OmniboxSuggestionButtonRowView&) =
      delete;
  OmniboxSuggestionButtonRowView& operator=(
      const OmniboxSuggestionButtonRowView&) = delete;
  ~OmniboxSuggestionButtonRowView() override;

  // Called when results background color is refreshed.
  void OnOmniboxBackgroundChange(SkColor omnibox_bg_color);

  // Updates the suggestion row buttons based on the model.
  void UpdateFromModel();

  views::Button* GetActiveButton() const;

 private:
  // Get the popup model from the view.
  const OmniboxPopupModel* model() const;

  // Digs into the model with index to get the match for owning result view.
  const AutocompleteMatch& match() const;

  void SetPillButtonVisibility(OmniboxSuggestionRowButton* button,
                               OmniboxPopupModel::LineState state);

  void ButtonPressed(OmniboxPopupModel::LineState state,
                     const ui::Event& event);

  OmniboxPopupContentsView* const popup_contents_view_;
  size_t const model_index_;

  OmniboxSuggestionRowButton* keyword_button_ = nullptr;
  OmniboxSuggestionRowButton* pedal_button_ = nullptr;
  OmniboxSuggestionRowButton* tab_switch_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
