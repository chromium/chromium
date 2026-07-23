// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
constexpr int kMaxAutocompleteMenuItems = 6;

}  // namespace

AutocompleteSuggestionGenerator::AutocompleteSuggestionGenerator(
    scoped_refptr<AutofillWebDataService> profile_database)
    : profile_database_(profile_database) {}

AutocompleteSuggestionGenerator::~AutocompleteSuggestionGenerator() {
  CancelPendingQuery();
}

struct AutocompleteSuggestionGenerator::QueryHandler {
  QueryHandler(
      std::u16string prefix,
      base::OnceCallback<void(ReturnedSuggestions)> on_suggestions_returned)
      : prefix(std::move(prefix)),
        on_suggestions_returned(std::move(on_suggestions_returned)) {}
  QueryHandler(QueryHandler&&) = default;
  QueryHandler& operator=(QueryHandler&&) = default;
  ~QueryHandler() = default;

  // Prefix used to search suggestions, submitted by the handler.
  std::u16string prefix;

  // Callback to-be-executed once a response from the DB is available.
  base::OnceCallback<void(ReturnedSuggestions)> on_suggestions_returned;
};

void AutocompleteSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  if (!trigger_field.should_autocomplete()) {
    std::move(callback).Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }

  auto is_autofillable = [ac_unrecognized_behavior =
                              GetAcUnrecognizedBehavior(client)](
                             const std::unique_ptr<AutofillField>& field) {
    return !field->ShouldSuppressSuggestionsAndFillingByDefault(
               ac_unrecognized_behavior) &&
           !field->Type().GetTypes().contains(UNKNOWN_TYPE);
  };
  // If Autofill (not Autocomplete) suggestions may be shown on some other field
  // of the form, we want to suppress Autocomplete suggestions on this field.
  if (trigger_autofill_field &&
      SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field, GetAcUnrecognizedBehavior(client)) &&
      std::ranges::any_of(*form_structure, is_autofillable)) {
    std::move(callback).Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }

  // Do not offer autocomplete suggestions for credit card number, cvc, and
  // expiration date related fields, including standalone CVC fields (used to
  // re-authenticate the use of a credit card the website has on file).
  if (FieldType type = trigger_autofill_field
                           ? trigger_autofill_field->Type().GetCreditCardType()
                           : UNKNOWN_TYPE;
      data_util::IsCreditCardExpirationType(type) ||
      type == CREDIT_CARD_VERIFICATION_CODE ||
      type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE ||
      type == CREDIT_CARD_NUMBER) {
    std::move(callback).Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }

  CancelPendingQuery();
  if (!AutocompleteHistoryManager::IsFieldNameMeaningfulForAutocomplete(
          trigger_field.name()) ||
      !client.IsAutocompleteEnabled() ||
      trigger_field.form_control_type() == FormControlType::kTextArea ||
      trigger_field.form_control_type() == FormControlType::kContentEditable ||
      IsInAutofillSuggestionsDisabledExperiment()) {
    std::move(callback).Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }

  if (!profile_database_) {
    std::move(callback).Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }
  // The permission to show the AtMemory button is checked now and passed into
  // the asynchronous callback. As a result, there is a marginal delay between
  // when the permission is checked and when the button is actually shown
  // (when the database read completes).
  bool is_at_memory_enabled =
      MayPerformAtMemoryAction(AtMemoryAction::kShowAutocompleteAtMemoryButton,
                               client,
                               client.GetLastCommittedPrimaryMainFrameURL()) &&
      MayPerformAtMemoryAction(AtMemoryAction::kShowAutocompleteAtMemoryButton,
                               client, trigger_field.origin().GetURL());

  auto on_autofill_values_returned =
      base::BindOnce(&AutocompleteSuggestionGenerator::OnAutofillValuesReturned,
                     weak_ptr_factory_.GetWeakPtr(),
                     QueryHandler(trigger_field.value(), std::move(callback)),
                     is_at_memory_enabled);
  if (base::FeatureList::IsEnabled(
          features::kAutofillLabelSensitiveAutocomplete)) {
    pending_query_ = profile_database_->GetFormValuesForElementNameAndLabel(
        trigger_field.name(), trigger_field.label(), trigger_field.value(),
        kMaxAutocompleteMenuItems, std::move(on_autofill_values_returned));
  } else {
    pending_query_ = profile_database_->GetFormValuesForElementName(
        trigger_field.name(), trigger_field.value(), kMaxAutocompleteMenuItems,
        std::move(on_autofill_values_returned));
  }
}

void AutocompleteSuggestionGenerator::OnAutofillValuesReturned(
    QueryHandler query_handler,
    bool is_at_memory_enabled,
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  if (!result) {
    // Returning early here if `result` is null.  We've seen this happen on
    // Linux due to NFS dismounting and causing SQL failures.
    // See http://crbug.com/68783.
    std::move(query_handler.on_suggestions_returned)
        .Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }
  DCHECK_EQ(result->GetType(),
            base::FeatureList::IsEnabled(
                features::kAutofillLabelSensitiveAutocomplete)
                ? AUTOCOMPLETE_SEARCH_RESULT
                : AUTOFILL_VALUE_RESULT);

  if (!pending_query_ || *pending_query_ != current_handle) {
    // There's no handler for this query, hence nothing to do.
    std::move(query_handler.on_suggestions_returned)
        .Run({SuggestionDataSource::kAutocomplete, {}});
    return;
  }
  // Removing the query, as it is no longer pending.
  pending_query_.reset();

  std::vector<Suggestion> suggestions;

  if (base::FeatureList::IsEnabled(
          features::kAutofillLabelSensitiveAutocomplete)) {
    const WDResult<std::vector<AutocompleteSearchResultLabelSensitive>>*
        autocomplete_result = static_cast<const WDResult<
            std::vector<AutocompleteSearchResultLabelSensitive>>*>(
            result.get());
    std::vector<AutocompleteSearchResultLabelSensitive> entries =
        autocomplete_result->GetValue();

    // If there are no entries or only one entry that is the exact same string
    // as what is in the input box, then don't offer it as a suggestion.
    if (entries.empty() || (entries.size() == 1 &&
                            query_handler.prefix == entries.front().value())) {
      std::move(query_handler.on_suggestions_returned)
          .Run({SuggestionDataSource::kAutocomplete, {}});
      return;
    }

    suggestions = base::ToVector(
        std::move(entries), [](AutocompleteSearchResultLabelSensitive& entry) {
          Suggestion suggestion(entry.value(),
                                SuggestionType::kAutocompleteEntry);
          suggestion.payload = std::move(entry);
          return suggestion;
        });
  } else {
    const WDResult<std::vector<AutocompleteEntry>>* autocomplete_result =
        static_cast<const WDResult<std::vector<AutocompleteEntry>>*>(
            result.get());
    std::vector<AutocompleteEntry> entries = autocomplete_result->GetValue();

    // If there are no entries or only one entry that is the exact same string
    // as what is in the input box, then don't offer it as a suggestion.
    if (entries.empty() ||
        (entries.size() == 1 &&
         query_handler.prefix == entries.front().key().value())) {
      std::move(query_handler.on_suggestions_returned)
          .Run({SuggestionDataSource::kAutocomplete, {}});
      return;
    }

    suggestions =
        base::ToVector(std::move(entries), [](AutocompleteEntry& entry) {
          Suggestion suggestion(entry.key().value(),
                                SuggestionType::kAutocompleteEntry);
          suggestion.payload = std::move(entry);
          return suggestion;
        });
  }
  if (is_at_memory_enabled &&
      base::FeatureList::IsEnabled(features::kShowAutocompleteAtMemoryButton)) {
    suggestions.emplace_back(SuggestionType::kSeparator);
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_TITLE),
        SuggestionType::kAutocompleteAtMemoryButton);
  }
  std::move(query_handler.on_suggestions_returned)
      .Run({SuggestionDataSource::kAutocomplete, std::move(suggestions)});
}

void AutocompleteSuggestionGenerator::CancelPendingQuery() {
  if (profile_database_ && pending_query_) {
    profile_database_->CancelRequest(*pending_query_);
  }
  pending_query_.reset();
}

bool AutocompleteSuggestionGenerator::HasPendingQuery() const {
  return pending_query_.has_value();
}

}  // namespace autofill
