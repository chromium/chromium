// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

// A `SuggestionGenerator` for `FillingProduct::kAutocomplete`. It provides
// suggestions based on the user's past entries.
//
// This generator fetches `AutocompleteEntry` data asynchronously from the
// `AutofillWebDataService`. It uses a `pending_query_` handle to manage the
// database request and the `OnAutofillValuesReturned` callback to receive the
// results. In the generation phase, it converts the fetched entries into
// `Suggestion` objects.
class AutocompleteSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit AutocompleteSuggestionGenerator(
      scoped_refptr<AutofillWebDataService> profile_database);
  ~AutocompleteSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  void CancelPendingQuery();
  bool HasPendingQuery() const;

  base::WeakPtr<AutocompleteSuggestionGenerator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Internal data class used to keep a request's context to associate it
  // with the appropriate response.
  struct QueryHandler;

  // Function handling WebDataService responses of type AUTOFILL_VALUE_RESULT.
  // `current_handle` is the DB query handle, and is used to retrieve the
  // handler associated with that query.
  // `result` contains the Autocomplete suggestions retrieved from the DB that,
  // if valid, will be passed to the callback in `query_handler`.
  void OnAutofillValuesReturned(QueryHandler query_handler,
                                WebDataServiceBase::Handle current_handle,
                                std::unique_ptr<WDTypedResult> result);

  scoped_refptr<AutofillWebDataService> profile_database_;

  // The handle of the current pending query to the WebDataService.
  // Since requests are asynchronous, this is used to identify the query when
  // its results are returned, preventing race conditions with old, stale
  // queries. It is also used to cancel a pending query if a new one is
  // initiated or if this manager is destroyed. It is `std::nullopt` if no query
  // is in flight.
  std::optional<WebDataServiceBase::Handle> pending_query_;

  base::WeakPtrFactory<AutocompleteSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GENERATOR_H_
