// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_SUGGESTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_SUGGESTION_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace autofill {

class PopupRowContentView;

class OmniboxAutofillSuggestion : public views::Button {
  METADATA_HEADER(OmniboxAutofillSuggestion, views::Button)
 public:
  OmniboxAutofillSuggestion(std::unique_ptr<PopupRowContentView> content_view,
                            const std::u16string& accessible_name,
                            PressedCallback pressed_callback,
                            base::RepeatingClosure selected_callback,
                            base::RepeatingClosure deselection_callback);
  OmniboxAutofillSuggestion(const OmniboxAutofillSuggestion&) = delete;
  OmniboxAutofillSuggestion& operator=(const OmniboxAutofillSuggestion&) =
      delete;
  ~OmniboxAutofillSuggestion() override;

  // views::View:
  void OnFocus() override;
  void OnBlur() override;

 protected:
  // views::Button:
  void StateChanged(ButtonState old_state) override;

 private:
  void UpdateSelectionState(bool selected);

  // The view wrapping the content area of the suggestion.
  raw_ptr<PopupRowContentView> content_view_;

  // Callback executed when the suggestion is selected (hovered).
  base::RepeatingClosure selected_callback_;

  // Callback executed when the suggestion is deselected (un-hovered).
  base::RepeatingClosure deselection_callback_;

  // If true, the suggestion is currently highlighted/active in the UI (i.e.,
  // hovered, focused, or pressed).
  bool selected_ = false;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_SUGGESTION_VIEW_H_
