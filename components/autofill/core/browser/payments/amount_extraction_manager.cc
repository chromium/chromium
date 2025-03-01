// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill::payments {

AmountExtractionManager::AmountExtractionManager(
    BrowserAutofillManager* autofill_manager)
    : autofill_manager_(CHECK_DEREF(autofill_manager)) {}

AmountExtractionManager::~AmountExtractionManager() = default;

// static
std::optional<uint64_t>
AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
    const std::string& amount) {
  const RE2 re(
      R"([^0-9,eE\-]*(0|[0-9]{1,3}(,?[0-9]{3})*)(\.([0-9]{2}))[^0-9eE\-]*)");
  std::string dollar;
  std::string cent;
  // The first regex capture group gives dollar and the fourth gives the cent.
  if (!RE2::FullMatch(amount, re, &dollar, nullptr, nullptr, &cent)) {
    return std::nullopt;
  }
  dollar.erase(std::remove(dollar.begin(), dollar.end(), ','), dollar.end());

  uint64_t dollar_value = 0;
  uint64_t cent_value = 0;
  base::StringToUint64(dollar, &dollar_value);
  base::StringToUint64(cent, &cent_value);

  // Safely multiply to convert amount to micro.
  uint64_t micro_amount = 0;
  base::CheckedNumeric<uint64_t> checked_dollar_value =
      base::CheckedNumeric<uint64_t>(dollar_value) * kMicrosPerDollar;
  base::CheckedNumeric<uint64_t> checked_cent_value =
      base::CheckedNumeric<uint64_t>(cent_value) * (kMicrosPerDollar / 100);
  base::CheckedNumeric<uint64_t> checked_result =
      checked_dollar_value + checked_cent_value;
  if (!checked_result.AssignIfValid(&micro_amount)) {
    return std::nullopt;
  }
  return micro_amount;
}

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
  // If the webpage is not in the amount extraction allowlist, do not trigger
  // the search.
  if (!IsUrlEligibleForAmountExtraction()) {
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
  const AmountExtractionHeuristicRegexes& heuristics =
      AmountExtractionHeuristicRegexes::GetInstance();
  GetMainFrameDriver()->ExtractLabeledTextNodeValue(
      base::UTF8ToUTF16(heuristics.amount_pattern()),
      base::UTF8ToUTF16(heuristics.keyword_pattern()),
      heuristics.number_of_ancestor_levels_to_search(),
      base::BindOnce(&AmountExtractionManager::OnCheckoutAmountReceived,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AmountExtractionManager::OnTimeoutReached,
                     weak_ptr_factory_.GetWeakPtr()),
      kAmountExtractionWaitTime);
}

void AmountExtractionManager::SetSearchRequestPendingForTesting(
    bool search_request_pending) {
  search_request_pending_ = search_request_pending;
}

bool AmountExtractionManager::GetSearchRequestPendingForTesting() {
  return search_request_pending_;
}

void AmountExtractionManager::OnCheckoutAmountReceived(
    base::TimeTicks search_request_start_timestamp,
    const std::string& extracted_amount) {
  autofill_metrics::LogAmountExtractionLatency(
      base::TimeTicks::Now() - search_request_start_timestamp,
      !extracted_amount.empty());
  autofill_metrics::LogAmountExtractionResult(
      extracted_amount.empty()
          ? autofill_metrics::AmountExtractionResult::kAmountNotFound
          : autofill_metrics::AmountExtractionResult::kSuccessful);
  // Set `search_request_pending_` to false once the search is done.
  search_request_pending_ = false;
  // Invalidate the WeakPtr instance to ignore the scheduled delay task when the
  // amount is found.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::optional<uint64_t> parsed_extracted_amount =
      MaybeParseAmountToMonetaryMicroUnits(extracted_amount);

  if (BnplManager* bnpl_manager = autofill_manager_->client()
                                      .GetPaymentsAutofillClient()
                                      ->GetPaymentsBnplManager()) {
    bnpl_manager->OnAmountExtractionReturned(parsed_extracted_amount);
  }
}

void AmountExtractionManager::OnTimeoutReached() {
  // If the amount is found, ignore this callback.
  if (!search_request_pending_) {
    return;
  }
  search_request_pending_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  autofill_metrics::LogAmountExtractionResult(
      autofill_metrics::AmountExtractionResult::kTimeout);
  // TODO(crbug.com/378517983): Add BNPL flow action logic here.
}

bool AmountExtractionManager::IsUrlEligibleForAmountExtraction() const {
  if (AutofillOptimizationGuide* autofill_optimization_guide =
          autofill_manager_->client().GetAutofillOptimizationGuide()) {
    const GURL& url =
        autofill_manager_->client().GetLastCommittedPrimaryMainFrameURL();
    for (std::string_view issuer : BnplManager::GetSupportedBnplIssuerIds()) {
      if (autofill_optimization_guide
              ->IsUrlEligibleForCheckoutAmountSearchForIssuerId(issuer, url)) {
        return true;
      }
    }
  }
  return false;
}

AutofillDriver* AmountExtractionManager::GetMainFrameDriver() {
  AutofillDriver* driver = &autofill_manager_->driver();
  while (driver->GetParent()) {
    driver = driver->GetParent();
  }
  return driver;
}

}  // namespace autofill::payments
