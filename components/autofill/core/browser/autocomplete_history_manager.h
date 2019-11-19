// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_subject.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

// Per-profile Autocomplete history manager. Handles receiving form data
// from the renderers and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public KeyedService,
                                   public WebDataServiceConsumer,
                                   public AutofillSubject {
 public:
  // Interface to be implemented by classes that want to fetch autocomplete
  // suggestions.
  class SuggestionsHandler {
   public:
    virtual ~SuggestionsHandler() = default;

    // Function that will be called-back once AutocompleteHistoryManager gets
    // the corresponding response from the DB.
    // |query_id| is the value given by the implementor when
    // OnGetAutocompleteSuggestions was called (it is not the DB query ID).
    // |suggestions| is the list of fetched autocomplete suggestions.
    virtual void OnSuggestionsReturned(
        int query_id,
        bool autoselect_first_suggestion,
        const std::vector<Suggestion>& suggestions) = 0;
  };

  AutocompleteHistoryManager();
  ~AutocompleteHistoryManager() override;

  // Initializes the instance with the given parameters.
  // |profile_database_| is a profile-scope DB used to access autocomplete data.
  // |is_off_the_record| indicates wheter the user is currently operating in an
  // off-the-record context (i.e. incognito).
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            PrefService* pref_service,
            bool is_off_the_record);

  // Returns a weak pointer to the current AutocompleteHistoryManager instance.
  base::WeakPtr<AutocompleteHistoryManager> GetWeakPtr();

  // Initiates a DB query to get suggestions given a field's information.
  // |query_id| is given by the client as context.
  // |is_autocomplete_enabled| is to determine if the feature is enable for the
  // requestor's context (e.g. Android WebViews have different contexts).
  // |name| is the name of the field,
  // |prefix| is the field's values
  // |form_control_type| is the field's control type.
  // |handler| is weak pointer to the requestor, which we will callback once we
  // receive the response. There can only be one pending query per |handler|,
  // hence this function will cancel the previous pending query if it hadn't
  // already been resolved, at which point no method of the handler will be
  // called.
  virtual void OnGetAutocompleteSuggestions(
      int query_id,
      bool is_autocomplete_enabled,
      bool autoselect_first_suggestion,
      const base::string16& name,
      const base::string16& prefix,
      const std::string& form_control_type,
      base::WeakPtr<SuggestionsHandler> handler);

  // Will save the given input from the |form| as a new, or updated,
  // autocomplete entry, which will be served in the future as a suggestion.
  // This update is dependent on whether we are running in incognito and if
  // autocomplete is enabled or not.
  virtual void OnWillSubmitForm(const FormData& form,
                                bool is_autocomplete_enabled);

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // Cancels the currently pending WebDataService queries associated with the
  // given |handler|.
  virtual void CancelPendingQueries(const SuggestionsHandler* handler);

  // Must be public for the autofill manager to use.
  virtual void OnRemoveAutocompleteEntry(const base::string16& name,
                                         const base::string16& value);

  // Invoked when the user selected |value| in the Autocomplete drop-down. This
  // function logs the DaysSinceLastUse of the Autocomplete entry associated
  // with |value|.
  virtual void OnAutocompleteEntrySelected(const base::string16& value);

 private:
  friend class AutocompleteHistoryManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteHistoryManagerTest,
                           AutocompleteUMAQueryCreated);

  // The class measure the percentage field that triggers the query and the
  // percentage field that has the suggestion.
  // TODO(crbug.com/908562): Move this to AutofillMetrics with the other
  // Autocomplete metrics for better consistency.
  class UMARecorder {
   public:
    UMARecorder() = default;
    ~UMARecorder() = default;

    void OnGetAutocompleteSuggestions(
        const base::string16& name,
        WebDataServiceBase::Handle pending_query_handle);
    void OnWebDataServiceRequestDone(
        WebDataServiceBase::Handle pending_query_handle,
        bool has_suggestion);

   private:
    // The query handle should be measured for UMA.
    WebDataServiceBase::Handle measuring_query_handle_ = 0;

    // The name of field that is currently measured, we don't repeatedly measure
    // the query of the same field while user is filling the field.
    base::string16 measuring_name_;

    DISALLOW_COPY_AND_ASSIGN(UMARecorder);
  };

  // Internal data object used to keep a request's context to associate it
  // with the appropriate response.
  struct QueryHandler {
    QueryHandler(int client_query_id,
                 bool autoselect_first_suggestion,
                 base::string16 prefix,
                 base::WeakPtr<SuggestionsHandler> handler);
    QueryHandler(const QueryHandler& original);
    ~QueryHandler();

    // Query ID living in the handler's scope, which is NOT the same as the
    // database query ID. This ID is unique per frame, but not per profile.
    int client_query_id_;

    // Determines whether we should auto-select the first suggestion when
    // returning. This value was given by the handler when requesting
    // suggestions.
    bool autoselect_first_suggestion_;

    // Prefix used to search suggestions, submitted by the handler.
    base::string16 prefix_;

    // Weak pointer to the handler instance which will be called-back when
    // we get the response for the associate query.
    base::WeakPtr<SuggestionsHandler> handler_;
  };

  // Sends the autocomplete |suggestions| to the |query_handler|'s handler for
  // display in the associated Autofill popup. The parameter may be empty if
  // there are no new autocomplete additions.
  void SendSuggestions(const std::vector<AutofillEntry>& entries,
                       const QueryHandler& query_handler);

  // Cancels all outstanding queries and clears out the |pending_queries_| map.
  void CancelAllPendingQueries();

  // Cleans-up the dictionary of |pending_queries_| by checking
  // - If any handler instance was destroyed (known via WeakPtr)
  // - If the given |handler| pointer is associated with a query.
  void CleanupEntries(const SuggestionsHandler* handler);

  // Function handling WebDataService responses of type AUTOFILL_VALUE_RESULT.
  // |current_handle| is the DB query handle, and is used to retrieve the
  // handler associated with that query.
  // |result| contains the Autocomplete suggestions retrieved from the DB that,
  // if valid and if the handler exists, are to be returned to the handler.
  void OnAutofillValuesReturned(WebDataServiceBase::Handle current_handle,
                                std::unique_ptr<WDTypedResult> result);

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

  // Map used to store WebDataService response callbacks, associating a
  // response's WDResultType to the appropriate callback.
  std::map<WDResultType,
           base::RepeatingCallback<void(WebDataServiceBase::Handle,
                                        std::unique_ptr<WDTypedResult>)>>
      request_callbacks_;

  // The PrefService that this instance uses. Must outlive this instance.
  PrefService* pref_service_;

  // When the manager makes a request from WebDataServiceBase, the database is
  // queried asynchronously. We associate the query handle to the requestor
  // (with some context parameters) and store the association here until we get
  // called back. Then we update the initial requestor, and deleting the
  // no-longer-pending query from this map.
  std::map<WebDataServiceBase::Handle, QueryHandler> pending_queries_;

  // Cached results of the last batch of autocomplete suggestions.
  // Key are the suggestions' values, and values are the associated
  // AutofillEntry.
  std::map<base::string16, AutofillEntry> last_entries_;

  // Whether the service is associated with an off-the-record browser context.
  bool is_off_the_record_ = false;

  UMARecorder uma_recorder_;

  base::WeakPtrFactory<AutocompleteHistoryManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutocompleteHistoryManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
