// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

class BrowserAutofillManager;
class FormData;
class FormFieldData;

// Controller for the accessibility annotator search feature. It handles queries
// to the AccessibilityQueryService and manages session-based metrics.
//
// Owned by `AutofillExternalDelegate`, its lifetime is tied to it.
class AtMemoryController {
 public:
  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  explicit AtMemoryController(BrowserAutofillManager& manager);
  ~AtMemoryController();

  AtMemoryController(const AtMemoryController&) = delete;
  AtMemoryController& operator=(const AtMemoryController&) = delete;

  // Called when suggestions are shown.
  void OnPopupShown(AutofillSuggestionTriggerSource trigger_source,
                    UpdateSuggestionsCallback update_callback);

  // Called when the user types in the filter/search bar. Returns true if the
  // current session is an @memory one.
  bool OnFilterChanged(const std::u16string& filter);

  // Called when the user has explicitly submitted the search. Returns true if
  // the current session is an @memory one.
  bool OnSearchSubmitted(const std::u16string& filter);

  // Called when suggestions are hidden.
  void OnPopupHidden();

  // Fills or previews the selected search result.
  void FillOrPreviewSearchResult(mojom::ActionPersistence action_persistence,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 const Suggestion& suggestion);

 private:
  void ExecuteQuery(const std::u16string& filter, bool full_search);

  const raw_ref<BrowserAutofillManager> manager_;

  std::optional<AutofillSuggestionTriggerSource> trigger_source_;

  UpdateSuggestionsCallback update_callback_;

  std::unique_ptr<AtMemoryFunnelMetrics> at_memory_funnel_metrics_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_CONTROLLER_H_
