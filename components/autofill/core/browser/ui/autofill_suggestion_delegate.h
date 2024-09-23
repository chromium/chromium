// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/common/aliases.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {
class PasswordManagerDriver;
}

namespace autofill {

class AutofillDriver;

// An interface for interaction with AutofillSuggestionController. It is notified
// of suggestion-related events by the controller.
class AutofillSuggestionDelegate {
 public:
  // Contains some additional information associated with a suggestion.
  struct SuggestionMetadata {
    // Defines the row selected in the list of suggestions.
    int row = 0;
    // On desktop, it defines the subpopup that contains the suggestion
    // selected.
    int sub_popup_level = 0;
    // Defines whether the suggestion appeared on a search result list (i.e.
    // the search input is not empty).
    bool from_search_result = false;
  };

  virtual ~AutofillSuggestionDelegate() = default;

  virtual absl::variant<AutofillDriver*,
                        password_manager::PasswordManagerDriver*>
  GetDriver() = 0;

  // Called when Autofill suggestions are shown. On Desktop, where the
  // suggestions support sub-popups, only the root popup triggers this call.
  virtual void OnSuggestionsShown(base::span<const Suggestion> suggestions) = 0;

  // Called when Autofill suggestions are hidden. This may also get called if
  // the suggestions were never shown at all, e.g. because of insufficient
  // space. On Desktop, only the root popup triggers this call.
  virtual void OnSuggestionsHidden() = 0;

  // Called when the autofill `suggestion` has been temporarily selected (e.g.,
  // hovered).
  virtual void DidSelectSuggestion(const Suggestion& suggestion) = 0;

  // Informs the delegate that a `suggestion` has been chosen.
  virtual void DidAcceptSuggestion(const Suggestion& suggestion,
                                   const SuggestionMetadata& metadata) = 0;

  // Informs the delegate that the user chose to perform the `button_action`
  // associated with `suggestion`. Actions are currently implemented only on
  // Desktop.
  virtual void DidPerformButtonActionForSuggestion(
      const Suggestion& suggestion,
      const SuggestionButtonAction& button_action) = 0;

  // Informs the delegate to delete the described suggestion. Returns true if
  // something was deleted, or false if deletion is not allowed.
  virtual bool RemoveSuggestion(const Suggestion& suggestion) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Returns the main filling product the popup being shown, which is a function
  // of the list of suggestions being shown.
  virtual FillingProduct GetMainFillingProduct() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
