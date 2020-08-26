// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"

class OmniboxPopupContentsView;
class OmniboxSuggestionRowButton;

// A view to contain the button row within a result view.
class OmniboxSuggestionButtonRowView : public views::View,
                                       public views::ButtonListener {
 public:
  explicit OmniboxSuggestionButtonRowView(OmniboxPopupContentsView* view,
                                          int model_index);
  ~OmniboxSuggestionButtonRowView() override;

  // Called when themes, styles, and visibility is refreshed in result view.
  void OnStyleRefresh();

  // Updates the suggestion row buttons based on the model.
  void UpdateFromModel();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Button* GetActiveButton() const;

 private:
  // Get the popup model from the view.
  const OmniboxPopupModel* model() const;

  // Digs into the model with index to get the match for owning result view.
  const AutocompleteMatch& match() const;

  void SetPillButtonVisibility(OmniboxSuggestionRowButton* button,
                               OmniboxPopupModel::LineState state);

  OmniboxPopupContentsView* const popup_contents_view_;
  size_t const model_index_;

  OmniboxSuggestionRowButton* keyword_button_ = nullptr;
  OmniboxSuggestionRowButton* pedal_button_ = nullptr;
  OmniboxSuggestionRowButton* tab_switch_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSuggestionButtonRowView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
