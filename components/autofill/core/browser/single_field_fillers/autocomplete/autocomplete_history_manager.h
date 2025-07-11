// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_AUTOCOMPLETE_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_AUTOCOMPLETE_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"

namespace autofill {

// Per-profile Autocomplete history manager. Handles receiving form data
// from the renderers and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public KeyedService {
 public:
  AutocompleteHistoryManager();

  AutocompleteHistoryManager(const AutocompleteHistoryManager&) = delete;
  AutocompleteHistoryManager& operator=(const AutocompleteHistoryManager&) =
      delete;

  ~AutocompleteHistoryManager() override;

  // Internal data object used to keep a request's context to associate it
  // with the appropriate response.
  struct QueryHandler {
    QueryHandler(FieldGlobalId field_id,
                 std::u16string prefix,
                 SingleFieldFillRouter::OnSuggestionsReturnedCallback
                     on_suggestions_returned);
    QueryHandler(const QueryHandler&) = delete;
    QueryHandler(QueryHandler&&);
    QueryHandler& operator=(const QueryHandler&) = delete;
    QueryHandler& operator=(QueryHandler&&);
    ~QueryHandler();

    // The queried field ID.
    FieldGlobalId field_id;

    // Prefix used to search suggestions, submitted by the handler.
    std::u16string prefix;

    // Callback to-be-executed once a response from the DB is available.
    SingleFieldFillRouter::OnSuggestionsReturnedCallback
        on_suggestions_returned;
  };

  // May generate autocomplete suggestions for the given `field`. This is
  // achieved through an async DB query. `client` checks if the requirements for
  // generating autocomplete suggestions are met (e.g. autocomplete is enabled).
  // If `OnGetSingleFieldSuggestions` decides to claim the opportunity to fill
  // `field`, it returns true and calls `on_suggestions_returned`. Claiming the
  // opportunity is not a promise that suggestions will be available. The
  // callback may be called with no suggestions.
  [[nodiscard]] virtual bool OnGetSingleFieldSuggestions(
      const FormFieldData& field,
      const AutofillClient& client,
      SingleFieldFillRouter::OnSuggestionsReturnedCallback&
          on_suggestions_returned);

  // Saves the `fields` that are eligible to be saved as new or updated
  // Autocomplete entries, which can then be served in the future as
  // suggestions. This update is dependent on whether we are running in
  // incognito and if Autocomplete is enabled or not. `fields` may be empty.
  virtual void OnWillSubmitFormWithFields(
      const std::vector<FormFieldData>& fields,
      bool is_autocomplete_enabled);

  virtual void CancelPendingQuery();

  virtual void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      SuggestionType type);

  virtual void OnSingleFieldSuggestionSelected(const Suggestion& suggestion);

  // Initializes the instance with the given parameters.
  // |profile_database_| is a profile-scope DB used to access autocomplete data.
  // |is_off_the_record| indicates wheter the user is currently operating in an
  // off-the-record context (i.e. incognito).
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            PrefService* pref_service,
            bool is_off_the_record);

  void OnWebDataServiceRequestDone(std::optional<QueryHandler> query_handler,
                                   WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result);

 private:
  friend class AutocompleteHistoryManagerTest;

  // Sends the autocomplete `entries` to the `query_handler` for display in the
  // associated Autofill popup. The parameter may be empty if there are no new
  // autocomplete additions.
  void SendSuggestions(const std::vector<AutocompleteEntry>& entries,
                       QueryHandler query_handler);

  // Function handling WebDataService responses of type AUTOFILL_VALUE_RESULT.
  // `current_handle` is the DB query handle, and is used to retrieve the
  // handler associated with that query.
  // `result` contains the Autocomplete suggestions retrieved from the DB that,
  // if valid, will be passed to the callback in `query_handler`.
  void OnAutofillValuesReturned(WebDataServiceBase::Handle current_handle,
                                std::unique_ptr<WDTypedResult> result,
                                QueryHandler query_handler);

  // Function handling WebDataService responses of type AUTOFILL_CLEANUP_RESULT.
  // |current_handle| is the DB query handle, and is used to retrieve the
  // handler associated with that query.
  // |result| contains the number of entries that were cleaned-up.
  void OnAutofillCleanupReturned(WebDataServiceBase::Handle current_handle,
                                 std::unique_ptr<WDTypedResult> result);

  // Returns true if the given |field| and its value are valid to be saved as a
  // new or updated Autocomplete entry.
  bool IsFieldValueSaveable(const FormFieldData& field);

  // Must outlive this object.
  scoped_refptr<AutofillWebDataService> profile_database_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_;

  // The handle of the current pending query to the WebDataService.
  // Since requests are asynchronous, this is used to identify the query when
  // its results are returned, preventing race conditions with old, stale queries.
  // It is also used to cancel a pending query if a new one is initiated or if
  // this manager is destroyed. It is `std::nullopt` if no query is in flight.
  std::optional<WebDataServiceBase::Handle> pending_query_;

  // Cached results of the last batch of autocomplete suggestions.
  // Key are the suggestions' values, and values are the associated
  // AutocompletEntry.
  std::map<std::u16string, AutocompleteEntry> last_entries_;

  // Whether the service is associated with an off-the-record browser context.
  bool is_off_the_record_ = false;

  base::WeakPtrFactory<AutocompleteHistoryManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_AUTOCOMPLETE_AUTOCOMPLETE_HISTORY_MANAGER_H_
