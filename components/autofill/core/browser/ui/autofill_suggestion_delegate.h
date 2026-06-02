// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_

#include <string>
#include <variant>

#include "base/containers/span.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"

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

    friend bool operator==(const SuggestionMetadata& lhs,
                           const SuggestionMetadata& rhs) = default;
  };

  virtual ~AutofillSuggestionDelegate() = default;

  // Called when the user has typed in the search bar.
  // Returns true if the delegate handles the filter change.
  virtual bool OnFilterChanged(const std::u16string& filter) = 0;

  // Called when the user has explicitly submitted the search (e.g. by hitting
  // Enter).
  // Returns true if the delegate handles the search submission.
  virtual bool OnSearchSubmitted(const std::u16string& filter) = 0;

  // Returns true if a search is currently in progress.
  virtual bool IsSearching() const = 0;

  virtual std::variant<AutofillDriver*,
                       password_manager::PasswordManagerDriver*>
  GetDriver() = 0;

  // Called when Autofill suggestions are shown. On Desktop, where the
  // suggestions support sub-popups, only the root popup triggers this call.
  virtual void OnSuggestionsShown(base::span<const Suggestion> suggestions) = 0;

  // Called when Autofill suggestions are hidden. This may also get called if
  // the suggestions were never shown at all, e.g. because of insufficient
  // space. On Desktop, only the root popup triggers this call.
  virtual void OnSuggestionsHidden(SuggestionHidingReason reason) = 0;

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

  // Called when `tab_type` is opened in the tabbed pane config of the autofill
  // dropdown.
  virtual void OnTabSelected(TabbedPaneTabType tab_type) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
