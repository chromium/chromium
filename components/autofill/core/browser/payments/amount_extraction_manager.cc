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
#include "components/autofill/core/browser/metrics/payments/ai_amount_extraction_metrics.h"
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

AiAmountExtractionResult::ResultType
AmountExtractionManager::ValidateAmountExtractionResponse(
    const optimization_guide::proto::AmountExtractionResponse& response) {
  std::optional<AiAmountExtractionResult::Error> error;

  // Lower priority check: currency. If checkout amount is missing or invalid,
  // this error will be overwritten later.
  if (!response.has_currency()) {
    error = AiAmountExtractionResult::Error::kMissingCurrency;
  } else if (response.currency() != "USD") {
    amount_extraction_status_.seen_unsupported_currency_for_page_load = true;
    error = AiAmountExtractionResult::Error::kUnsupportedCurrency;
  }

  // Higher priority check: checkout amount. If it is missing or invalid, the
  // error code will be overwritten.
  if (!response.has_final_checkout_amount()) {
    error = AiAmountExtractionResult::Error::kAmountMissing;
  } else if (response.final_checkout_amount() < 0) {
    error = AiAmountExtractionResult::Error::kNegativeAmount;
  }

  if (error.has_value()) {
    return base::unexpected(*error);
  }

  int64_t amount_in_micros =
      static_cast<int64_t>(response.final_checkout_amount() * kMicrosPerDollar);

  return std::make_pair(amount_in_micros, response.currency());
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
  CHECK(base::FeatureList::IsEnabled(
      features::kAutofillEnableAiBasedAmountExtraction));
  ai_amount_extraction_start_time_ = base::TimeTicks::Now();

  autofill_manager_->client().GetAiPageContent(
      base::BindOnce(&AmountExtractionManager::OnAiPageContentReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AmountExtractionManager::OnAiPageContentReceived(
    std::optional<optimization_guide::proto::AnnotatedPageContent> result) {
  if (!has_logged_apc_fetch_result_) {
    autofill_metrics::LogAiAmountExtractionApcFetchResult(
        /*success=*/result.has_value(),
        GetMainFrameDriver()->GetPageUkmSourceId());
    has_logged_apc_fetch_result_ = true;
  }

  if (!result) {
    if (BnplManager* bnpl_manager =
            autofill_manager_->GetPaymentsBnplManager()) {
      bnpl_manager->OnAmountExtractionReturnedFromAi(base::unexpected(
          AiAmountExtractionResult::Error::kFailureToGenerateApc));
    }
    // Stop the timer because amount extraction is finished with a failure.
    Reset();
    return;
  }

  optimization_guide::proto::AmountExtractionRequest request;
  *request.mutable_annotated_page_content() = std::move(*result);

  autofill_manager_->client().GetRemoteModelExecutor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kAmountExtraction,
      std::move(request),
      {.execution_timeout = kAiBasedAmountExtractionWaitTime},
      base::BindOnce(&AmountExtractionManager::OnCheckoutAmountReceivedFromAi,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AmountExtractionManager::TriggerCheckoutAmountExtractionWithAi() {
  // In case of timeout, cancel the request and show the error dialog.
  timeout_timer_.Start(
      FROM_HERE, kAiBasedAmountExtractionWaitTime,
      base::BindOnce(&AmountExtractionManager::OnTimeoutReached,
                     weak_ptr_factory_.GetWeakPtr()));

  FetchAiPageContent();
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

bool AmountExtractionManager::HasTimedOutForPageLoad() const {
  return amount_extraction_status_.has_timed_out_for_page_load;
}

bool AmountExtractionManager::SeenUnsupportedCurrencyForPageLoad() const {
  return amount_extraction_status_.seen_unsupported_currency_for_page_load;
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

  Reset();
}

void AmountExtractionManager::OnCheckoutAmountReceivedFromAi(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // If no timeout, it means the server response came back in time, stop the
  // timer.
  timeout_timer_.Stop();

  CHECK(ai_amount_extraction_start_time_.has_value());
  base::TimeDelta latency =
      base::TimeTicks::Now() - ai_amount_extraction_start_time_.value();
  ai_amount_extraction_start_time_.reset();

  BnplManager* bnpl_manager = autofill_manager_->GetPaymentsBnplManager();
  if (!bnpl_manager) {
    Reset();
    return;
  }

  const std::optional<optimization_guide::proto::AmountExtractionResponse>
      response = result.response.has_value()
                     ? optimization_guide::ParsedAnyMetadata<
                           optimization_guide::proto::AmountExtractionResponse>(
                           *result.response)
                     : std::nullopt;

  AiAmountExtractionResult::ResultType extraction_result;
  if (!response.has_value()) {
    extraction_result = base::unexpected(
        AiAmountExtractionResult::Error::kMissingServerResponse);
  } else {
    extraction_result = ValidateAmountExtractionResponse(response.value());
  }

  LogAiAmountExtractionResultIfApplicable(extraction_result, latency);
  bnpl_manager->OnAmountExtractionReturnedFromAi(std::move(extraction_result));
  Reset();
}

void AmountExtractionManager::OnTimeoutReached() {
  amount_extraction_status_.has_timed_out_for_page_load = true;
  // Once timeout is reached, cancel all the pending function calls.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (base::FeatureList::IsEnabled(
          ::autofill::features::kAutofillEnableAiBasedAmountExtraction)) {
    AiAmountExtractionResult::ResultType result =
        base::unexpected(AiAmountExtractionResult::Error::kTimeout);
    if (BnplManager* bnpl_manager =
            autofill_manager_->GetPaymentsBnplManager()) {
      bnpl_manager->OnAmountExtractionReturnedFromAi(result);
    }
    LogAiAmountExtractionResultIfApplicable(result, /*latency=*/std::nullopt);
  } else {
    // If the amount is found, ignore this callback.
    if (!search_request_pending_) {
      return;
    }
    search_request_pending_ = false;
    if (BnplManager* bnpl_manager =
            autofill_manager_->GetPaymentsBnplManager()) {
      bnpl_manager->OnAmountExtractionReturned(
          /*extracted_amount=*/std::nullopt,
          /*timeout_reached=*/true);
    }
    if (!has_logged_amount_extraction_result_) {
      autofill_metrics::LogAmountExtractionResult(
          /*latency=*/std::nullopt,
          autofill_metrics::AmountExtractionResult::kTimeout,
          GetMainFrameDriver()->GetPageUkmSourceId());
      has_logged_amount_extraction_result_ = true;
    }
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

  Reset();
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

void AmountExtractionManager::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_.Stop();
  search_request_pending_ = false;
}

void AmountExtractionManager::LogAiAmountExtractionResultIfApplicable(
    AiAmountExtractionResult::ResultType result,
    std::optional<base::TimeDelta> latency) {
  if (!has_logged_amount_extraction_result_) {
    autofill_metrics::LogAiAmountExtractionResult(
        result, latency, GetMainFrameDriver()->GetPageUkmSourceId());
    has_logged_amount_extraction_result_ = true;
  }
}

}  // namespace autofill::payments
