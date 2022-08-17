// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/iban_manager.h"

#include "base/containers/contains.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

IBANManager::IBANManager(PersonalDataManager* personal_data_manager,
                         bool is_off_the_record)
    : personal_data_manager_(personal_data_manager),
      is_off_the_record_(is_off_the_record) {}

IBANManager::~IBANManager() = default;

void IBANManager::OnGetSingleFieldSuggestions(
    int query_id,
    bool is_autocomplete_enabled,
    bool autoselect_first_suggestion,
    const FormFieldData& field,
    base::WeakPtr<SuggestionsHandler> handler,
    const SuggestionsContext& context) {
  if (!is_off_the_record_ && personal_data_manager_) {
    std::vector<IBAN*> ibans = personal_data_manager_->GetIBANs();
    if (!ibans.empty()) {
      // Rank the IBANs by ranking score (see AutoFillDataModel for details).
      base::Time comparison_time = AutofillClock::Now();
      base::ranges::sort(
          ibans, [comparison_time](const IBAN* iban0, const IBAN* iban1) {
            return iban0->HasGreaterRankingThan(iban1, comparison_time);
          });
      SendIBANSuggestions(ibans,
                          QueryHandler(query_id, autoselect_first_suggestion,
                                       field.value, handler));
    }
  }
}

base::WeakPtr<IBANManager> IBANManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void IBANManager::SendIBANSuggestions(const std::vector<IBAN*>& ibans,
                                      const QueryHandler& query_handler) {
  if (!query_handler.handler_) {
    // Either the handler has been destroyed, or it is invalid.
    return;
  }

  // If the input box content equals any of the available IBANs, then
  // assume the IBAN has been filled, and don't show any suggestions.
  // Note: this |prefix_| is actually the value of form and we are comparing
  // the value with the full IBAN value. However, once we land
  // MASKED_SERVER_IBANs and Chrome doesn't know the whole value, we'll have
  // check 'prefix'(E.g., the first ~5 characters).
  if (base::Contains(ibans, query_handler.prefix_, &IBAN::value))
    return;

  // Return suggestions to query handler.
  query_handler.handler_->OnSuggestionsReturned(
      query_handler.client_query_id_,
      query_handler.autoselect_first_suggestion_,
      AutofillSuggestionGenerator::GetSuggestionsForIBANs(ibans));
}

}  // namespace autofill
