// Copyright 2022 The Chromium Authors
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

bool IBANManager::OnGetSingleFieldSuggestions(
    AutoselectFirstSuggestion autoselect_first_suggestion,
    const FormFieldData& field,
    const AutofillClient& client,
    base::WeakPtr<SuggestionsHandler> handler,
    const SuggestionsContext& context) {
  if (!is_off_the_record_ && personal_data_manager_) {
    std::vector<IBAN*> ibans = personal_data_manager_->GetLocalIBANs();
    if (!ibans.empty()) {
      // Rank the IBANs by ranking score (see AutoFillDataModel for details).
      base::Time comparison_time = AutofillClock::Now();
      base::ranges::sort(
          ibans, [comparison_time](const IBAN* iban0, const IBAN* iban1) {
            return iban0->HasGreaterRankingThan(iban1, comparison_time);
          });
      SendIBANSuggestions(
          ibans, QueryHandler(field.global_id(), autoselect_first_suggestion,
                              field.value, handler));
      return true;
    }
  }
  return false;
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
  if (base::Contains(ibans, query_handler.prefix_, &IBAN::value)) {
    // Return empty suggestions to query handler. This will result in no
    // suggestions being displayed.
    query_handler.handler_->OnSuggestionsReturned(
        query_handler.field_id_, query_handler.autoselect_first_suggestion_,
        {});
    return;
  }

  // Return suggestions to query handler.
  query_handler.handler_->OnSuggestionsReturned(
      query_handler.field_id_, query_handler.autoselect_first_suggestion_,
      AutofillSuggestionGenerator::GetSuggestionsForIBANs(ibans));
}

}  // namespace autofill
