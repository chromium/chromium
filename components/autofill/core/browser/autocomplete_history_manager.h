// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_subject.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

struct SuggestionsContext;

// Per-profile Autocomplete history manager. Handles receiving form data
// from the renderers and the storing and retrieving of form data
// through WebDataServiceBase.
class AutocompleteHistoryManager : public SingleFieldFormFiller,
                                   public KeyedService,
                                   public WebDataServiceConsumer,
                                   public AutofillSubject {
 public:
  AutocompleteHistoryManager();

  AutocompleteHistoryManager(const AutocompleteHistoryManager&) = delete;
  AutocompleteHistoryManager& operator=(const AutocompleteHistoryManager&) =
      delete;

  ~AutocompleteHistoryManager() override;

  // SingleFieldFormFiller overrides:
  [[nodiscard]] bool OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource trigger_source,
      const FormFieldData& field,
      const AutofillClient& client,
      base::WeakPtr<SuggestionsHandler> handler,
      const SuggestionsContext& context) override;
  void OnWillSubmitFormWithFields(const std::vector<FormFieldData>& fields,
                                  bool is_autocomplete_enabled) override;
  void CancelPendingQueries(const SuggestionsHandler* handler) override;
  void OnRemoveCurrentSingleFieldSuggestion(const std::u16string& field_name,
                                            const std::u16string& value,
                                            PopupItemId popup_item_id) override;
  void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                       PopupItemId popup_item_id) override;

  // Initializes the instance with the given parameters.
  // |profile_database_| is a profile-scope DB used to access autocomplete data.
  // |is_off_the_record| indicates wheter the user is currently operating in an
  // off-the-record context (i.e. incognito).
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            PrefService* pref_service,
            bool is_off_the_record);

  // Returns a weak pointer to the current AutocompleteHistoryManager instance.
  base::WeakPtr<AutocompleteHistoryManager> GetWeakPtr();

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

 private:
  friend class AutocompleteHistoryManagerTest;

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
  raw_ptr<PrefService> pref_service_;

  // When the manager makes a request from WebDataServiceBase, the database is
  // queried asynchronously. We associate the query handle to the requestor
  // (with some context parameters) and store the association here until we get
  // called back. Then we update the initial requestor, and deleting the
  // no-longer-pending query from this map.
  std::map<WebDataServiceBase::Handle, QueryHandler> pending_queries_;

  // Cached results of the last batch of autocomplete suggestions.
  // Key are the suggestions' values, and values are the associated
  // AutofillEntry.
  std::map<std::u16string, AutofillEntry> last_entries_;

  // Whether the service is associated with an off-the-record browser context.
  bool is_off_the_record_ = false;

  base::WeakPtrFactory<AutocompleteHistoryManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOCOMPLETE_HISTORY_MANAGER_H_
