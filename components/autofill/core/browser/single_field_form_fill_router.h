// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_form_filler.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

// Routes single field form filling requests, such as choosing whether to direct
// them to Autocomplete or merchant promo code filling functionality.
class SingleFieldFormFillRouter : public SingleFieldFormFiller {
 public:
  explicit SingleFieldFormFillRouter(
      AutocompleteHistoryManager* autocomplete_history_manager);
  ~SingleFieldFormFillRouter() override;
  SingleFieldFormFillRouter(const SingleFieldFormFillRouter&) = delete;
  SingleFieldFormFillRouter& operator=(const SingleFieldFormFillRouter&) =
      delete;

  // SingleFieldFormFiller overrides:
  void OnGetSingleFieldSuggestions(
      int query_id,
      bool is_autocomplete_enabled,
      bool autoselect_first_suggestion,
      const std::u16string& name,
      const std::u16string& prefix,
      const std::string& form_control_type,
      base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler)
      override;
  void OnWillSubmitForm(const FormData& form,
                        bool is_autocomplete_enabled) override;
  void CancelPendingQueries(
      const SingleFieldFormFiller::SuggestionsHandler* handler) override;
  void OnRemoveCurrentSingleFieldSuggestion(
      const std::u16string& field_name,
      const std::u16string& value) override;
  void OnSingleFieldSuggestionSelected(const std::u16string& value) override;

 private:
  // The SingleFieldFormFiller being used to fill the current field. Reset
  // whenever suggestions are requested for a new field.
  base::WeakPtr<SingleFieldFormFiller> current_single_field_form_filler_;

  // Available single field form fillers:
  base::WeakPtr<AutocompleteHistoryManager> autocomplete_history_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SINGLE_FIELD_FORM_FILL_ROUTER_H_
