// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillClient;
class AutofillField;
class FormStructure;

// Interface for form-filling implementations that fill a single field at a
// time, such as autocomplete or merchant promo codes.
class SingleFieldFormFiller {
 public:
  // Some `SingleFieldFormFillers` return suggestions asynchronously. This
  // callback is used to eventually return suggestions. `field_id` identifies
  // the field the query refer to. `suggestions` is the list of fetched
  // suggestions.
  // TODO(crbug.com/40100455): This should be a `base::OnceCallback<>`. It is
  // currently a repeating callback, because the `SingleFieldFormFillRouter`
  // asks all available `SingleFieldFormFiller`s using
  // `OnGetSingleFieldSuggestions()`, until the first one returns true. This
  // requires passing the callback to all `SingleFieldFormFillers` (even though
  // only one of them will end up calling it).
  using OnSuggestionsReturnedCallback =
      base::RepeatingCallback<void(FieldGlobalId,
                                   const std::vector<Suggestion>&)>;

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
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      const AutofillClient& client,
      OnSuggestionsReturnedCallback on_suggestions_returned) = 0;

  // Runs when a form is going to be submitted. In the case of Autocomplete, it
  // saves the given |fields| that are eligible to be saved as new or updated
  // Autocomplete entries, which can then be served in the future as
  // suggestions. This update is dependent on whether we are running in
  // incognito and if Autocomplete is enabled or not. |fields| can be empty.
  virtual void OnWillSubmitFormWithFields(
      const std::vector<FormFieldData>& fields,
      bool is_autocomplete_enabled) = 0;

  // Cancels all pending queries. This is only applicable to
  // `SingleFieldFormFillers`` that fetch suggestions asynchronously.
  virtual void CancelPendingQueries() = 0;

  // If applicable, removes the currently-selected suggestion from the database.
  // `type` is the SuggestionType of the suggestion to be
  // removed.
  virtual void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value,
      SuggestionType type) = 0;

  // Invoked when the user selects `suggestion` in the list of suggestions. For
  // Autocomplete, this function logs the DaysSinceLastUse of the Autocomplete
  // entry associated with the value of the suggestion.
  virtual void OnSingleFieldSuggestionSelected(
      const Suggestion& suggestion) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
