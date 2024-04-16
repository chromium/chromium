// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

namespace autofill {

// The interface implemented by Desktop implementations of the
// `AutofillSuggestionController` - that is, the methods found below are
// specific to Desktop.
class AutofillPopupController : public AutofillSuggestionController {
 public:
  // Creates and shows a sub-popup adjacent to `anchor_bounds`. The sub-popup
  // represents another level of `suggestions` which must be semantically
  // connected to a parent level suggestion, e.g. an address suggestion
  // break down providing more granular fillings.
  // The popup created via this method and this popup instance are linked
  // as child-parent. The child's lifetime depends on its parent, i.e. when
  // the parent dies the child dies also.
  virtual base::WeakPtr<AutofillSuggestionController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions,
      AutoselectFirstSuggestion autoselect_first_suggestion) = 0;

  // Hides open by `OpenSubPopup()` popup, noop if there is no open sub-popup.
  virtual void HideSubPopup() = 0;

  // Returns whether the popup should ignore the check that the mouse was
  // observed out of bounds - see `PopupRowView` for more detail.
  virtual bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const = 0;

  // Executes the action associated with the button that is displayed in the
  // suggestion at `index`. Button actions depend on the type of the suggestion.
  virtual void PerformButtonActionForSuggestion(int index) = 0;

  virtual base::WeakPtr<AutofillPopupController> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_H_
