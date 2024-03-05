// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {
class PasswordManagerDriver;
}

namespace autofill {

class AutofillDriver;

// An interface for interaction with AutofillPopupController. Will be notified
// of events by the controller.
class AutofillPopupDelegate {
 public:
  // Defines the position of the suggestion that was selected.
  // This is useful for desktop where popups can have sub-popups.
  struct SuggestionPosition {
    // Defines the row selected in the list of suggestions.
    int row = 0;
    // On desktop, it defines the subpopup that contains the suggestion
    // selected.
    int sub_popup_level = 0;
  };

  virtual ~AutofillPopupDelegate() = default;

  virtual absl::variant<AutofillDriver*,
                        password_manager::PasswordManagerDriver*>
  GetDriver() = 0;

  // Called when the Autofill popup is shown. If the popup supports sub-popups
  // only the root one triggers it.
  virtual void OnPopupShown() = 0;

  // Called when the Autofill popup is hidden. This may also get called if the
  // popup was never shown at all, e.g. because of insufficient space.
  // If the popup supports sub-popups only the root one triggers it.
  virtual void OnPopupHidden() = 0;

  // Called when the autofill `suggestion` has been temporarily selected (e.g.,
  // hovered).
  virtual void DidSelectSuggestion(const Suggestion& suggestion) = 0;

  // Informs the delegate that a row in the popup has been chosen. `suggestion`
  // is the suggestion that was chosen in the popup. `position` refers to the
  // row and level of the suggestion in the suggestions layout.
  virtual void DidAcceptSuggestion(const Suggestion& suggestion,
                                   const SuggestionPosition& position) = 0;

  // Informs the delegate that the user chose to perform the button action
  // associated with `suggestion`. Actions are currently implemented only on
  // Desktop.
  virtual void DidPerformButtonActionForSuggestion(
      const Suggestion& suggestion) = 0;

  // Delete the described suggestion. Returns true if something was deleted,
  // or false if deletion is not allowed.
  virtual bool RemoveSuggestion(const Suggestion& suggestion) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Returns the main filling product the popup being shown, which is a function
  // of the list of suggestions being shown.
  virtual FillingProduct GetMainFillingProduct() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_POPUP_DELEGATE_H_
