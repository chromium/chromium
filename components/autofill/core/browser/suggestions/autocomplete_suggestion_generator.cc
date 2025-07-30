// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"

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
  QueryHandler(FieldGlobalId field_id,
               std::u16string prefix,
               base::OnceCallback<void(
                   std::pair<FillingProduct,
                             std::vector<SuggestionGenerator::SuggestionData>>)>
                   on_suggestions_returned)
      : field_id(field_id),
        prefix(std::move(prefix)),
        on_suggestions_returned(std::move(on_suggestions_returned)) {}
  QueryHandler(QueryHandler&&) = default;
  QueryHandler& operator=(QueryHandler&&) = default;
  ~QueryHandler() = default;

  // The queried field ID.
  FieldGlobalId field_id;

  // Prefix used to search suggestions, submitted by the handler.
  std::u16string prefix;

  // Callback to-be-executed once a response from the DB is available.
  base::OnceCallback<void(
      std::pair<FillingProduct,
                std::vector<SuggestionGenerator::SuggestionData>>)>
      on_suggestions_returned;
};

void AutocompleteSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!field_data.should_autocomplete()) {
    std::move(callback).Run({FillingProduct::kAutocomplete, {}});
    return;
  }

  CancelPendingQuery();
  if (!AutocompleteHistoryManager::IsFieldNameMeaningfulForAutocomplete(
          field_data.name()) ||
      !client.IsAutocompleteEnabled() ||
      field_data.form_control_type() == FormControlType::kTextArea ||
      field_data.form_control_type() == FormControlType::kContentEditable ||
      IsInAutofillSuggestionsDisabledExperiment()) {
    std::move(callback).Run({FillingProduct::kAutocomplete, {}});
    return;
  }

  if (!profile_database_) {
    std::move(callback).Run({FillingProduct::kAutocomplete, {}});
    return;
  }

  pending_query_ = profile_database_->GetFormValuesForElementName(
      field_data.name(), field_data.value(), kMaxAutocompleteMenuItems,
      base::BindOnce(&AutocompleteSuggestionGenerator::OnAutofillValuesReturned,
                     weak_ptr_factory_.GetWeakPtr(),
                     QueryHandler(field_data.global_id(), field_data.value(),
                                  std::move(callback))));
}

void AutocompleteSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  std::vector<SuggestionData> autocomplete_suggestion_data =
      ExtractSuggestionDataForFillingProduct(all_suggestion_data,
                                             FillingProduct::kAutocomplete);

  std::vector<AutocompleteEntry> autocomplete_entries =
      base::ToVector(std::move(autocomplete_suggestion_data),
                     [](SuggestionData& suggestion_data) {
                       return std::get<autofill::AutocompleteEntry>(
                           std::move(suggestion_data));
                     });

  // If there is only one suggestion that is the exact same string as
  // what is in the input box, then don't show the suggestion.
  if (autocomplete_entries.size() == 1 &&
      field_data.value() == autocomplete_entries[0].key().value()) {
    std::move(callback).Run({FillingProduct::kAutocomplete, {}});
    return;
  }

  std::vector<Suggestion> suggestions;
  suggestions.reserve(autocomplete_entries.size());
  for (const AutocompleteEntry& entry : autocomplete_entries) {
    suggestions.emplace_back(entry.key().value(),
                             SuggestionType::kAutocompleteEntry);
    suggestions.back().payload = std::move(entry);
  }
  std::move(callback).Run(
      {FillingProduct::kAutocomplete, std::move(suggestions)});
}

void AutocompleteSuggestionGenerator::OnAutofillValuesReturned(
    QueryHandler query_handler,
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  if (!result) {
    // Returning early here if `result` is null.  We've seen this happen on
    // Linux due to NFS dismounting and causing sql failures.
    // See http://crbug.com/68783.
    std::move(query_handler.on_suggestions_returned)
        .Run({FillingProduct::kAutocomplete, {}});
    return;
  }
  DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());

  if (!pending_query_ || *pending_query_ != current_handle) {
    // There's no handler for this query, hence nothing to do.
    std::move(query_handler.on_suggestions_returned)
        .Run({FillingProduct::kAutocomplete, {}});
    return;
  }
  // Removing the query, as it is no longer pending.
  pending_query_.reset();

  const WDResult<std::vector<AutocompleteEntry>>* autocomplete_result =
      static_cast<const WDResult<std::vector<AutocompleteEntry>>*>(
          result.get());
  std::vector<AutocompleteEntry> entries = autocomplete_result->GetValue();
  std::vector<SuggestionGenerator::SuggestionData> suggestion_data =
      base::ToVector(std::move(entries), [](AutocompleteEntry& entry) {
        return SuggestionGenerator::SuggestionData(std::move(entry));
      });
  std::move(query_handler.on_suggestions_returned)
      .Run({FillingProduct::kAutocomplete, std::move(suggestion_data)});
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
