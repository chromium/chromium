// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/common/aliases.h"
#include "components/input/native_web_keyboard_event.h"

namespace autofill {

class AutofillSuggestionController;

// The interface for creating and controlling a platform-dependent
// AutofillPopupView.
class AutofillPopupView {
 public:
  struct SearchBarConfig {
    std::u16string placeholder;
    std::u16string no_results_message;
  };

  // Factory function for creating the view. Providing `std::nullopt` to
  // the `search_bar_config` results in creating a popup without a search bar.
  static base::WeakPtr<AutofillPopupView> Create(
      base::WeakPtr<AutofillSuggestionController> controller,
      std::optional<const SearchBarConfig> search_bar_config = std::nullopt);

  // Attempts to display the Autofill popup and fills it with data from the
  // controller. Returns whether the popup was shown.
  virtual bool Show(AutoselectFirstSuggestion autoselect_first_suggestion) = 0;

  // Hides the popup from view. This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // Handles a key press event and returns whether the event should be
  // swallowed. This allows views to handle events that depend on its internal
  // state, such as changing the selected Autofill cell.
  virtual bool HandleKeyPressEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Refreshes the position and redraws popup when suggestions change. Returns
  // whether the resulting popup was shown (or had to hide, e.g. due to
  // insufficient size). If `prefer_prev_arrow_side` is `true`, the view takes
  // prev arrow side as the first preferred when recalculating the popup
  // position (potentially changed due to the change of the content).
  virtual void OnSuggestionsChanged(bool prefer_prev_arrow_side) = 0;

  // Returns true if the autofill popup overlaps with the
  // picture-in-picture window.
  virtual bool OverlapsWithPictureInPictureWindow() const = 0;

  // Makes accessibility announcement.
  virtual void AxAnnounce(const std::u16string& text) = 0;

  // Return the autofill popup view's ax unique id.
  virtual std::optional<int32_t> GetAxUniqueId() = 0;

  // Creates a sub-popup (child) view linked to this (parent) view.
  // The child's lifetime depends on its parent, i.e. when the parent dies
  // the child dies also.
  virtual base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillSuggestionController> sub_controller) = 0;

  virtual std::optional<AutofillClient::PopupScreenLocation>
  GetPopupScreenLocation() const = 0;

  // Indicates whether any of the view elements currently has focus.
  virtual bool HasFocus() const = 0;

  // Returns a weak pointer to itself.
  virtual base::WeakPtr<AutofillPopupView> GetWeakPtr() = 0;

 protected:
  virtual ~AutofillPopupView() {}
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
