// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillClient;
struct SuggestionsContext;

// Interface for form-filling implementations that fill a single field at a
// time, such as autocomplete or merchant promo codes.
class SingleFieldFormFiller {
 public:
  // Interface to be implemented by classes that want to fetch autocomplete
  // suggestions.
  class SuggestionsHandler {
   public:
    virtual ~SuggestionsHandler() = default;

    // Will be called-back once SingleFieldFormFiller gets the corresponding
    // response from the DB. `field_id` identifies the field the query refers
    // to. `suggestions` is the list of fetched suggestions.
    virtual void OnSuggestionsReturned(
        FieldGlobalId field_id,
        AutoselectFirstSuggestion autoselect_first_suggestion,
        const std::vector<Suggestion>& suggestions) = 0;
  };

  SingleFieldFormFiller();
  virtual ~SingleFieldFormFiller();
  SingleFieldFormFiller(const SingleFieldFormFiller&) = delete;
  SingleFieldFormFiller& operator=(const SingleFieldFormFiller&) = delete;

  // Gets suggestions for a given field. In the case of Autocomplete, this is
  // through a DB query, though it could be different for other fill types.
  // `client` is used for functionality such as checking if autocomplete is
  // enabled, or checking if the URL we navigated to is blocklisted for the
  // specific single field form filler that we are trying to retrieve
  // suggestions from. `field` is the given field. `handler` is weak pointer to
  // the requestor, which we will call back once we receive the response. There
  // can only be one pending query per `handler`, hence this function will
  // cancel the previous pending query if it hadn't already been resolved, at
  // which point no method of the handler will be called. The boolean return
  // value denotes whether a SingleFieldFormFiller claims the opportunity to
  // fill this field. By returning true, the SingleFieldFormFiller does not
  // promise at this point to have a value available for filling. It just
  // promises to call back the handler and voids the opportunity for other
  // SingleFieldFormFillers to offer filling the field. The callback can happen
  // synchronously even before OnGetSingleFieldSuggestions returns true.
  [[nodiscard]] virtual bool OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion autoselect_first_suggestion,
      const FormFieldData& field,
      const AutofillClient& client,
      base::WeakPtr<SuggestionsHandler> handler,
      const SuggestionsContext& context) = 0;

  // Runs when a form is going to be submitted. In the case of Autocomplete, it
  // saves the given |fields| that are eligible to be saved as new or updated
  // Autocomplete entries, which can then be served in the future as
  // suggestions. This update is dependent on whether we are running in
  // incognito and if Autocomplete is enabled or not. |fields| can be empty.
  virtual void OnWillSubmitFormWithFields(
      const std::vector<FormFieldData>& fields,
      bool is_autocomplete_enabled) = 0;

  // Cancels the currently pending WebDataService queries associated with the
  // given |handler|.
  virtual void CancelPendingQueries(const SuggestionsHandler* handler) = 0;

  // If applicable, removes the currently-selected suggestion from the database.
  // |frontend_id| is the PopupItemId of the suggestion to be removed.
  virtual void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      int frontend_id) = 0;

  // Invoked when the user selects |value| in the list of suggestions. For
  // Autocomplete, this function logs the DaysSinceLastUse of the Autocomplete
  // entry associated with |value|. |frontend_id| is the PopupItemId of the
  // suggestion selected.
  virtual void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                               int frontend_id) = 0;

 protected:
  // Internal data object used to keep a request's context to associate it
  // with the appropriate response.
  struct QueryHandler {
    QueryHandler(FieldGlobalId field_id,
                 AutoselectFirstSuggestion autoselect_first_suggestion,
                 std::u16string prefix,
                 base::WeakPtr<SuggestionsHandler> handler);
    QueryHandler(const QueryHandler& original);
    ~QueryHandler();

    // The queried field ID.
    FieldGlobalId field_id_;

    // Determines whether we should auto-select the first suggestion when
    // returning. This value was given by the handler when requesting
    // suggestions.
    AutoselectFirstSuggestion autoselect_first_suggestion_;

    // Prefix used to search suggestions, submitted by the handler.
    std::u16string prefix_;

    // Weak pointer to the handler instance which will be called-back when
    // we get the response for the associate query.
    base::WeakPtr<SuggestionsHandler> handler_;
  };
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
