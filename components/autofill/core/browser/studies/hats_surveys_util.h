// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_

#include <map>
#include <optional>
#include <string>

#include "base/containers/lru_cache.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class AutofillField;
class FormStructure;

// Type of key-value pairs that can be sent with a HaTS survey as metadata
// (product specific data, PSD). The keys must be configured for each survey in
// the survey configuration at `//chrome/browser/ui/hats/survey_config.cc`.
using HatsSurveyStringData = std::map<std::string, std::string>;

// If the conditions of a HaTS survey are satisfied, try to trigger the survey
// about Autofill that is related to a recently submitted form. This does not
// necessarily cause a survey to be shown to the user since the surveys are
// hidden behind a probability check. This function relies on
// `AutofillField::possible_types()` being set for the fields in
// `submitted_form`.
void MaybeTriggerFormSubmissionHatsSurveys(AutofillClient& client,
                                           const FormStructure& submitted_form);

// Keeps track of the most recent user interactions in the context of
// `AutofillAiManager`. This information is used for collecting the
// product-specific data (PSD) of HaTS surveys.
class RecentUserAutofillAiInteractionsForHats final {
 public:
  struct InteractionDetails {
    std::optional<EntityType> entity_type_accepted;
    std::optional<EntityInstance::RecordType> accepted_entity_record_type;
    // The types of the field where the suggestion was shown or accepted.
    FieldTypeSet autofill_ai_field_types;
  };

  static constexpr size_t kSuggestionInteractionMemorySize = 5;

  RecentUserAutofillAiInteractionsForHats();
  ~RecentUserAutofillAiInteractionsForHats();

  void SuggestionsShown(const FormStructure& form, const AutofillField& field);

  void SuggestionAccepted(const FormStructure& form,
                          const EntityInstance& entity);

  std::optional<InteractionDetails> GetRecentUserInteraction(
      FormGlobalId form_id) const;

 private:
  // Keeps suggestions details about the most recent forms the user has
  // interacted with.
  base::LRUCache<FormGlobalId, InteractionDetails>
      user_suggestion_interactions_per_form_{kSuggestionInteractionMemorySize};
};

void MaybeTriggerAutofillAiSubmissionHatsSurveys(
    AutofillClient& client,
    const FormStructure& submitted_form,
    const RecentUserAutofillAiInteractionsForHats& suggestion_interactions);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STUDIES_HATS_SURVEYS_UTIL_H_
