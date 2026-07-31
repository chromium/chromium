// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_suggestion_view.h"

#include <utility>

#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

OmniboxAutofillSuggestion::OmniboxAutofillSuggestion(
    std::unique_ptr<PopupRowContentView> content_view,
    const std::u16string& accessible_name,
    PressedCallback pressed_callback,
    base::RepeatingClosure selected_callback,
    base::RepeatingClosure deselection_callback)
    : views::Button(std::move(pressed_callback)),
      content_view_(AddChildView(std::move(content_view))),
      selected_callback_(std::move(selected_callback)),
      deselection_callback_(std::move(deselection_callback)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetRequestFocusOnPress(true);
  SetAccessibleName(accessible_name);
  SetNotifyEnterExitOnChild(true);
}

OmniboxAutofillSuggestion::~OmniboxAutofillSuggestion() = default;

void OmniboxAutofillSuggestion::OnFocus() {
  views::Button::OnFocus();
  UpdateSelectionState(true);
}

void OmniboxAutofillSuggestion::OnBlur() {
  views::Button::OnBlur();
  UpdateSelectionState(false);
}

void OmniboxAutofillSuggestion::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  bool selected =
      GetState() == STATE_HOVERED || GetState() == STATE_PRESSED || HasFocus();
  UpdateSelectionState(selected);
}

void OmniboxAutofillSuggestion::UpdateSelectionState(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  content_view_->UpdateStyle(selected_);
  if (selected_) {
    if (selected_callback_) {
      selected_callback_.Run();
    }
  } else {
    if (deselection_callback_) {
      deselection_callback_.Run();
    }
  }
}

BEGIN_METADATA(OmniboxAutofillSuggestion)
END_METADATA

}  // namespace autofill
