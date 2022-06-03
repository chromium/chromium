// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

// Interface for form-filling implementations that fill a single field at a
// time, such as Autocomplete or merchant promo codes.
class SingleFieldFormFiller {
 public:
  // Interface to be implemented by classes that want to fetch autocomplete
  // suggestions.
  class SuggestionsHandler {
   public:
    virtual ~SuggestionsHandler() = default;

    // Function that will be called-back once SingleFieldFormFiller gets the
    // corresponding response from the DB.
    // |query_id| is the value given by the implementor when
    // OnGetSingleFieldSuggestions was called (it is not the DB query ID).
    // |suggestions| is the list of fetched suggestions.
    virtual void OnSuggestionsReturned(
        int query_id,
        bool autoselect_first_suggestion,
        const std::vector<Suggestion>& suggestions) = 0;
  };

  SingleFieldFormFiller();
  virtual ~SingleFieldFormFiller();
  SingleFieldFormFiller(const SingleFieldFormFiller&) = delete;
  SingleFieldFormFiller& operator=(const SingleFieldFormFiller&) = delete;

  // Gets suggestions for a given field. In the case of Autocomplete, this is
  // through a DB query, though it could be different for other fill types.
  // |query_id| is given by the client as context.
  // |is_autocomplete_enabled| is to determine if the feature is enable for the
  // requestor's context (e.g. Android WebViews have different contexts).
  // |name| is the name of the field.
  // |prefix| is the field's value.
  // |form_control_type| is the field's control type.
  // |handler| is weak pointer to the requestor, which we will call back once we
  // receive the response. There can only be one pending query per |handler|,
  // hence this function will cancel the previous pending query if it hadn't
  // already been resolved, at which point no method of the handler will be
  // called.
  virtual void OnGetSingleFieldSuggestions(
      int query_id,
      bool is_autocomplete_enabled,
      bool autoselect_first_suggestion,
      const std::u16string& name,
      const std::u16string& prefix,
      const std::string& form_control_type,
      base::WeakPtr<SuggestionsHandler> handler) = 0;

  // Runs when a form is going to be submitted. In the case of Autocomplete, it
  // saves the given input from the |form| as new or updated Autocomplete
  // entries, which can then be served in the future as suggestions. This update
  // is dependent on whether we are running in incognito and if Autocomplete is
  // enabled or not.
  virtual void OnWillSubmitForm(const FormData& form,
                                bool is_autocomplete_enabled) = 0;

  // Cancels the currently pending WebDataService queries associated with the
  // given |handler|.
  virtual void CancelPendingQueries(const SuggestionsHandler* handler) = 0;

  // If applicable, removes the currently-selected suggestion from the database.
  virtual void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value) = 0;

  // Invoked when the user selects |value| in the list of suggestions. For
  // Autocomplete, this function logs the DaysSinceLastUse of the Autocomplete
  // entry associated with |value|.
  virtual void OnSingleFieldSuggestionSelected(const std::u16string& value) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILLER_H_
