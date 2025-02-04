// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

AmountExtractionManager::AmountExtractionManager(
    BrowserAutofillManager* autofill_manager)
    : autofill_manager_(CHECK_DEREF(autofill_manager)) {}

AmountExtractionManager::~AmountExtractionManager() = default;

bool AmountExtractionManager::ShouldTriggerAmountExtraction(
    const SuggestionsContext& context,
    bool should_suppress_suggestions,
    bool has_suggestions) const {
  // If there is an ongoing search, do not trigger the search.
  if (search_request_pending_) {
    return false;
  }
  // If autofill is not available, do not offer BNPL.
  if (!context.is_autofill_available) {
    return false;
  }
  // If there are no suggestions, do not show a BNPL chip as suggestions showing
  // is a requirement for BNPL.
  if (!has_suggestions) {
    return false;
  }
  // If there are no suggestions, do not show a BNPL chip as suggestions showing
  // is a requirement for BNPL.
  if (should_suppress_suggestions) {
    return false;
  }
  // BNPL is only offered for Credit Card filling scenarios.
  if (context.filling_product != FillingProduct::kCreditCard) {
    return false;
  }

  // TODO(crbug.com/378531706) check that there is at least one BNPL issuer
  // present.
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS)) {
    return base::FeatureList::IsEnabled(
        ::autofill::features::kAutofillEnableAmountExtractionDesktop);
  } else {
    return false;
  }
}

void AmountExtractionManager::TriggerCheckoutAmountExtraction() {
  if (search_request_pending_) {
    return;
  }
  search_request_pending_ = true;
  GetMainFrameDriver()->ExtractLabeledTextNodeValue(
      base::UTF8ToUTF16(
          AmountExtractionHeuristicRegexes::GetInstance().amount_pattern()),
      base::UTF8ToUTF16(
          AmountExtractionHeuristicRegexes::GetInstance().keyword_pattern()),
      AmountExtractionHeuristicRegexes::GetInstance()
          .number_of_ancestor_levels_to_search(),
      base::BindOnce(&AmountExtractionManager::OnCheckoutAmountReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AmountExtractionManager::SetSearchRequestPendingForTesting(
    bool search_request_pending) {
  search_request_pending_ = search_request_pending;
}

void AmountExtractionManager::OnCheckoutAmountReceived(
    const std::string& extracted_amount) {
  // Set `search_request_pending_` to false once the search is done.
  search_request_pending_ = false;
  // TODO(crbug.com/378517983): Add BNPL flow action logic here.
}

AutofillDriver* AmountExtractionManager::GetMainFrameDriver() {
  AutofillDriver* driver = &autofill_manager_->driver();
  while (driver->GetParent()) {
    driver = driver->GetParent();
  }
  return driver;
}

}  // namespace autofill::payments
