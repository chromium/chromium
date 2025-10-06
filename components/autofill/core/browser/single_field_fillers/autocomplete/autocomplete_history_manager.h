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
#include "components/autofill/core/browser/suggestions/autocomplete_suggestion_generator.h"
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

  // Generates autocomplete suggestions for the given `trigger_field` in `form`.
  // This is achieved through an async DB query. `client` checks if the
  // requirements for generating autocomplete suggestions are met (e.g.
  // autocomplete is enabled). Since autocomplete suggestions are always
  // generated last, the `on_suggestions_returned` callback may be called with
  // the suggestions for `field` or with an empty vector if no suggestions are
  // available.
  // TODO(crbug.com/409962888): Remove this method once the new suggestion
  // generation flow is launched.
  virtual void OnGetSingleFieldSuggestions(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& trigger_field,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      SingleFieldFillRouter::OnSuggestionsReturnedCallback
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

  // Returns true if the field has a meaningful `name`.
  // An input field name 'field_2' bears no semantic meaning and there is a
  // chance that a different website or different form uses the same field name
  // for a totally different purpose.
  static bool IsFieldNameMeaningfulForAutocomplete(const std::u16string& name);

  // Function handling WebDataService responses of type AUTOFILL_CLEANUP_RESULT.
  // `current_handle` is the DB query handle, and is used to retrieve the
  // handler associated with that query.
  // `result` contains the number of entries that were cleaned-up, it is
  // currently unused.
  void OnAutofillCleanupReturned(WebDataServiceBase::Handle current_handle,
      std::unique_ptr<WDTypedResult> result);

  scoped_refptr<AutofillWebDataService> GetProfileDatabase() {
    return profile_database_;
  }

 private:
  friend class AutocompleteHistoryManagerTest;

  // Returns true if the given |field| and its value are valid to be saved as a
  // new or updated Autocomplete entry.
  bool IsFieldValueSaveable(const FormFieldData& field);

  // Must outlive this object.
  scoped_refptr<AutofillWebDataService> profile_database_;

  // Stores the currently used `AutocompleteSuggestionGenerator`.
  // If a new `GetSingleFieldSuggestions` request is received, the previous
  // `suggestion_generator_` is destroyed and a new one is created.
  std::unique_ptr<AutocompleteSuggestionGenerator> suggestion_generator_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_;


  // Whether the service is associated with an off-the-record browser context.
  bool is_off_the_record_ = false;

  base::WeakPtrFactory<AutocompleteHistoryManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FILLERS_AUTOCOMPLETE_AUTOCOMPLETE_HISTORY_MANAGER_H_
