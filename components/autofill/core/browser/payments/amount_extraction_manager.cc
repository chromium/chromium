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
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"
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
  std::erase(dollar, ',');

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

DenseSet<AmountExtractionManager::EligibleFeature>
AmountExtractionManager::GetEligibleFeatures(const SuggestionsContext& context,
                                             bool should_suppress_suggestions,
                                             bool has_suggestions,
                                             FieldType field_type) const {
  // If there is an ongoing search, do not trigger the search.
  if (search_request_pending_) {
    return {};
  }
  // If autofill is not available, do not trigger the search.
  if (!context.is_autofill_available) {
    return {};
  }

  // If the interacted form field is CVC, do not trigger the search.
  if (kCvcFieldTypes.find(field_type) != kCvcFieldTypes.end()) {
    return {};
  }

  // If there are no suggestions, do not trigger the search as suggestions
  // showing is a requirement for amount extraction.
  if (!has_suggestions) {
    return {};
  }
  // If there are no suggestions, do not trigger the search as suggestions
  // showing is a requirement for amount extraction.
  if (should_suppress_suggestions) {
    return {};
  }
  // Amount extraction is only offered for Credit Card filling scenarios.
  if (context.filling_product != FillingProduct::kCreditCard) {
    return {};
  }

  // None of the projects that use amount extraction are intended to be enabled
  // in off-the-record mode, so do not run amount extraction in off-the-record
  // mode.
  if (autofill_manager_->client().IsOffTheRecord()) {
    return {};
  }

  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS)) {
    if (base::FeatureList::IsEnabled(
            ::autofill::features::
                kAutofillEnableAmountExtractionDesktopLogging)) {
      // Insert all amount extraction eligible features for logging.
      return DenseSet<EligibleFeature>::all();
    }
  }

  const DenseSet<EligibleFeature> eligible_features =
      CheckEligiblilityForFeaturesRequiringAmountExtraction();

  // Run after all other feature eligibilities are checked to only check feature
  // flag for eligible users.
  // TODO(crbug.com/414648193): Rename amount extraction feature flag to
  // remove the platform restriction.
  if (!eligible_features.empty() &&
      base::FeatureList::IsEnabled(
          ::autofill::features::kAutofillEnableAmountExtractionDesktop)) {
    return eligible_features;
  }

  return {};
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
  base::TimeDelta latency =
      base::TimeTicks::Now() - search_request_start_timestamp;
  autofill_metrics::LogAmountExtractionLatency(latency,
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

  if (BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager()) {
    bnpl_manager->OnAmountExtractionReturned(parsed_extracted_amount);
  }
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS)) {
    if (base::FeatureList::IsEnabled(
            ::autofill::features::
                kAutofillEnableAmountExtractionDesktopLogging)) {
      VLOG(3) << "The result of amount extraction on domain "
              << autofill_manager_->client()
                     .GetLastCommittedPrimaryMainFrameOrigin()
              << " is " << extracted_amount << " with latency of "
              << latency.InMilliseconds() << " milliseconds.";
    }
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
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS)) {
    if (base::FeatureList::IsEnabled(
            ::autofill::features::
                kAutofillEnableAmountExtractionDesktopLogging)) {
      VLOG(3) << "The amount extraction on domain "
              << autofill_manager_->client()
                     .GetLastCommittedPrimaryMainFrameOrigin()
              << " reached a timeout.";
    }
  }
}

DenseSet<AmountExtractionManager::EligibleFeature>
AmountExtractionManager::CheckEligiblilityForFeaturesRequiringAmountExtraction()
    const {
  DenseSet<EligibleFeature> eligible_features;

  // Check eligibility of BNPL feature.
  // Currently, BNPL is only offered for desktop platforms.
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS)) {
    if (BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager();
        bnpl_manager && bnpl_manager->IsEligibleForBnpl()) {
      eligible_features.insert(EligibleFeature::kBnpl);
    }
  }

  return eligible_features;
}

AutofillDriver* AmountExtractionManager::GetMainFrameDriver() {
  AutofillDriver* driver = &autofill_manager_->driver();
  while (driver->GetParent()) {
    driver = driver->GetParent();
  }
  return driver;
}

}  // namespace autofill::payments
