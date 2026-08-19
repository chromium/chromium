// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_

#include <string>
#include <variant>

#include "base/check.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"
#include "components/autofill/core/common/unique_ids.h"

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
    int row() const {
      CHECK(!multi_index.empty());
      return multi_index.back();
    }

    size_t sub_popup_level() const {
      CHECK(!multi_index.empty());
      return multi_index.size() - 1;
    }

    // The (multi-)index of the selected suggestion. It contains all the indices
    // of the selected suggestion from the root popup down to the suggestion
    // that was selected. Examples:
    // - If the suggestion with index `n` of the root popup was selected, this
    //   is `{n}`.
    // - If the suggestion with index `p` of a child popup anchored on the
    //   suggestion of index `n` in the root popup was selected, this is
    //   `{n, p}`.
    std::vector<size_t> multi_index;

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

  // Will be removed together with kAutofillSimplifyFocusCheck.
  virtual std::variant<AutofillDriver*,
                       password_manager::PasswordManagerDriver*>
  GetDriver_DoNotUse() = 0;

  // Called when Autofill `suggestions` are shown.
  // `parent_suggestion_metadata` contains metadata about the parent suggestion
  // serving as the anchor for the sub-popup (or its equivalent on mobile).
  // It is `std::nullopt` if `suggestions` are showing at the root level.
  virtual void OnSuggestionsShown(base::span<const Suggestion> suggestions,
                                  base::optional_ref<const SuggestionMetadata>
                                      parent_suggestion_metadata) = 0;

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

  // Returns the global ID of the field for which suggestions are being queried.
  virtual FieldGlobalId GetQueriedFieldId() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_SUGGESTION_DELEGATE_H_
