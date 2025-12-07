// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/amount_extraction.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill::payments {

AmountExtractionManager::AmountExtractionManager(
    BrowserAutofillManager* autofill_manager)
    : autofill_manager_(CHECK_DEREF(autofill_manager)) {}

AmountExtractionManager::~AmountExtractionManager() = default;

// static
std::optional<int64_t>
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

  int64_t dollar_value = 0;
  int64_t cent_value = 0;
  base::StringToInt64(dollar, &dollar_value);
  base::StringToInt64(cent, &cent_value);

  // Safely multiply to convert amount to micro.
  int64_t micro_amount = 0;
  base::CheckedNumeric<int64_t> checked_dollar_value =
      base::CheckedNumeric<int64_t>(dollar_value) * kMicrosPerDollar;
  base::CheckedNumeric<int64_t> checked_cent_value =
      base::CheckedNumeric<int64_t>(cent_value) * (kMicrosPerDollar / 100);
  base::CheckedNumeric<int64_t> checked_result =
      checked_dollar_value + checked_cent_value;
  if (!checked_result.AssignIfValid(&micro_amount)) {
    return std::nullopt;
  }
  return micro_amount;
}

bool AmountExtractionManager::IsValidAmountExtractionResponse(
    const AmountExtractionResponse& response) {
  // TODO(crbug.com/444683986): Log the metric for the invalid amount extraction
  // predication in the invalid cases.
  if (!response.has_final_checkout_amount()) {
    return false;
  }

  // The final checkout amount should never be negative.
  if (response.final_checkout_amount() < 0) {
    return false;
  }

  if (!response.has_currency()) {
    return false;
  }

  if (!base::IsStringASCII(response.currency())) {
    return false;
  }

  // ISO 4217 is always 3-letter.
  if (response.currency().length() != 3) {
    return false;
  }

  // ISO 4217 is always upper case.
  // Don't uppercase this code to proceed. It could convert invalid code into a
  // valid one. For example \u00DFP (Eszett+P) becomes SSP.
  if ((!base::IsAsciiUpper(response.currency()[0])) ||
      (!base::IsAsciiUpper(response.currency()[1])) ||
      (!base::IsAsciiUpper(response.currency()[2]))) {
    return false;
  }

  return true;
}

DenseSet<AmountExtractionManager::EligibleFeature>
AmountExtractionManager::GetEligibleFeatures(
    bool is_autofill_payments_enabled,
    bool should_suppress_suggestions,
    const std::vector<Suggestion>& suggestions,
    FillingProduct filling_product,
    FieldType field_type) const {
  // In AI-based amount extraction case, if there is a BNPL suggestion present,
  // then the amount extraction flow should be initiated.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAiBasedAmountExtraction)) {
    if (std::ranges::none_of(suggestions, [](const Suggestion& suggestion) {
          return suggestion.type == SuggestionType::kBnplEntry;
        })) {
      return {};
    }
  } else {
    // If there is an ongoing search, do not trigger the search.
    if (search_request_pending_) {
      return {};
    }
    // If autofill is not available, do not trigger the search.
    if (!is_autofill_payments_enabled) {
      return {};
    }

    // If the interacted form field is CVC, do not trigger the search.
    if (kCvcFieldTypes.find(field_type) != kCvcFieldTypes.end()) {
      return {};
    }

    // If there are no suggestions, do not trigger the search as suggestions
    // showing is a requirement for amount extraction.
    if (suggestions.empty()) {
      return {};
    }
    // If there are no suggestions, do not trigger the search as suggestions
    // showing is a requirement for amount extraction.
    if (should_suppress_suggestions) {
      return {};
    }
    // Amount extraction is only offered for Credit Card filling scenarios.
    if (filling_product != FillingProduct::kCreditCard) {
      return {};
    }
  }

  const DenseSet<EligibleFeature> eligible_features =
      CheckEligibilityForFeaturesRequiringAmountExtraction();

  // Run after all other feature eligibilities are checked to only check feature
  // flag for eligible users.
  if (!eligible_features.empty() &&
      base::FeatureList::IsEnabled(
          ::autofill::features::kAutofillEnableAmountExtraction)) {
    return eligible_features;
  }

  return {};
}

void AmountExtractionManager::FetchAiPageContent() {
  if (is_fetching_ai_page_content_) {
    return;
  }
  is_fetching_ai_page_content_ = true;
  autofill_manager_->client().GetAiPageContent(
      base::BindOnce(&AmountExtractionManager::OnAiPageContentReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AmountExtractionManager::OnAiPageContentReceived(
    std::optional<optimization_guide::proto::AnnotatedPageContent> result) {
  if (result) {
    ai_page_content_ =
        std::make_unique<optimization_guide::proto::AnnotatedPageContent>(
            std::move(*result));
  }
  is_fetching_ai_page_content_ = false;
  // TODO(crbug.com/444683986): Log ApcGenerationResult to UMA.
}

void AmountExtractionManager::TriggerCheckoutAmountExtractionWithAi() {
  if (!ai_page_content_) {
    // TODO(crbug.com/444685164) If the member variable `ai_page_content_` is
    // not initialized, another attempt to fetch it will be made. Retry only
    // once.
    return;
  }

  // Construct request
  optimization_guide::proto::AmountExtractionRequest request;
  *request.mutable_annotated_page_content() = std::move(*ai_page_content_);
  ai_page_content_.reset();

  autofill_manager_->client().GetRemoteModelExecutor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kAmountExtraction,
      std::move(request),
      {.execution_timeout = kAiBasedAmountExtractionWaitTime},
      base::BindOnce(&AmountExtractionManager::OnCheckoutAmountReceivedFromAi,
                     weak_ptr_factory_.GetWeakPtr()));
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

void AmountExtractionManager::OnCheckoutAmountReceived(
    base::TimeTicks search_request_start_timestamp,
    const std::string& extracted_amount) {
  base::TimeDelta latency =
      base::TimeTicks::Now() - search_request_start_timestamp;
  autofill_metrics::AmountExtractionResult result =
      extracted_amount.empty()
          ? autofill_metrics::AmountExtractionResult::kAmountNotFound
          : autofill_metrics::AmountExtractionResult::kSuccessful;
  if (!has_logged_amount_extraction_result_) {
    autofill_metrics::LogAmountExtractionResult(
        latency, result, GetMainFrameDriver()->GetPageUkmSourceId());
    has_logged_amount_extraction_result_ = true;
  }
  // Set `search_request_pending_` to false once the search is done.
  search_request_pending_ = false;
  // Invalidate the WeakPtr instance to ignore the scheduled delay task when the
  // amount is found.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::optional<int64_t> parsed_extracted_amount =
      MaybeParseAmountToMonetaryMicroUnits(extracted_amount);

  if (BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager()) {
    bnpl_manager->OnAmountExtractionReturned(parsed_extracted_amount,
                                             /*timeout_reached=*/false);
  }
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)) {
    if (base::FeatureList::IsEnabled(
            ::autofill::features::kAutofillEnableAmountExtractionTesting)) {
      VLOG(3) << "The result of amount extraction on domain "
              << autofill_manager_->client()
                     .GetLastCommittedPrimaryMainFrameOrigin()
              << " is " << extracted_amount << " with latency of "
              << latency.InMilliseconds() << " milliseconds.";
    }
  }
}

void AmountExtractionManager::OnCheckoutAmountReceivedFromAi(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.response.has_value()) {
    return;
  }

  std::optional<optimization_guide::proto::AmountExtractionResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::AmountExtractionResponse>(
          result.response.value());

  if (!response) {
    return;
  }

  BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager();

  if (!bnpl_manager) {
    return;
  }

  if (!IsValidAmountExtractionResponse(response.value())) {
    bnpl_manager->OnAmountExtractionReturnedFromAi(std::nullopt,
                                                   /*timeout_reached=*/false);
    return;
  }

  int64_t parsed_extracted_amount = static_cast<int64_t>(
      response->final_checkout_amount() * kMicrosPerDollar);

  bnpl_manager->OnAmountExtractionReturnedFromAi(parsed_extracted_amount,
                                                 /*timeout_reached=*/false);
}

void AmountExtractionManager::OnTimeoutReached() {
  // If the amount is found, ignore this callback.
  if (!search_request_pending_) {
    return;
  }
  search_request_pending_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!has_logged_amount_extraction_result_) {
    autofill_metrics::LogAmountExtractionResult(
        /*latency=*/std::nullopt,
        autofill_metrics::AmountExtractionResult::kTimeout,
        GetMainFrameDriver()->GetPageUkmSourceId());
    has_logged_amount_extraction_result_ = true;
  }
  if (BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager()) {
    bnpl_manager->OnAmountExtractionReturned(/*extracted_amount=*/std::nullopt,
                                             /*timeout_reached=*/true);
  }
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)) {
    if (base::FeatureList::IsEnabled(
            ::autofill::features::kAutofillEnableAmountExtractionTesting)) {
      VLOG(3) << "The amount extraction on domain "
              << autofill_manager_->client()
                     .GetLastCommittedPrimaryMainFrameOrigin()
              << " reached a timeout.";
    }
  }
}

DenseSet<AmountExtractionManager::EligibleFeature>
AmountExtractionManager::CheckEligibilityForFeaturesRequiringAmountExtraction()
    const {
  DenseSet<EligibleFeature> eligible_features;

  // Check eligibility of BNPL feature.
  if constexpr (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
                BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)) {
    if (IsEligibleForBnpl(autofill_manager_->client())) {
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
