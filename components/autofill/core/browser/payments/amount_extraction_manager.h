// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/optimization_guide/proto/features/amount_extraction.pb.h"

namespace autofill {
class AutofillDriver;
class BrowserAutofillManager;

namespace autofill_metrics {
enum class AiAmountExtractionResult;
}  // namespace autofill_metrics

}  // namespace autofill

namespace optimization_guide {

namespace proto {
class AnnotatedPageContent;
}  // namespace proto

class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace autofill::payments {

// Encapsulates the result of the AI-based amount extraction process.
// This uses base::expected to enforce explicit handling of both the success
// path (valid amount and currency) and specific failure cases. This
// distinguishs between a total failure to find an amount and a specific issue
// with the currency.
struct AiAmountExtractionResult {
  using AmountAndCurrency = std::pair<int64_t, std::string>;

  enum class Error {
    kFailureToGenerateApc = 0,
    kMissingServerResponse = 1,
    kNegativeAmount = 2,
    kAmountMissing = 3,
    kMissingCurrency = 4,
    kUnsupportedCurrency = 5,
    kTimeout = 6,
  };

  using ResultType = base::expected<AmountAndCurrency, Error>;
};

// Status flags of the checkout amount extraction from the page.
struct AmountExtractionStatus {
  // Whether the attempt to extract the checkout amount timed out.
  bool has_timed_out_for_page_load = false;
  // Whether an unsupported currency was detected during the amount extraction
  // process.
  bool seen_unsupported_currency_for_page_load = false;
};

// Owned by `BrowserAutofillManager`. This class manages the flow of the
// checkout amount extraction. The amount extraction flow starts from
// `BrowserAutofillManager`, which will call `AmountExtractionManager` to check
// whether a search should be triggered. If eligible, `AmountExtractionManager`
// will ask the main frame `AutofillDriver` to search for the checkout amount
// from the main frame DOM tree because it's where the final checkout amount
// lives. Since the search may take some time, it does not block credit card
// suggestion from being shown on UI because the IPC call between the browser
// process and renderer process is non-blocking. Once the result is passed back,
// `AmountExtractionManager` will pass the result to `BnplManager` and let it
// check whether this value is eligible for a Buy Now, Pay Later(BNPL) offer. If
// so, a new BNPL chip and the existing credit card suggestions will be shown on
// UI together. If not, it does nothing.
class AmountExtractionManager {
 public:
  using AmountExtractionResponse =
      optimization_guide::proto::AmountExtractionResponse;

  // Enum for all features that require amount extraction.
  enum class EligibleFeature {
    // Buy now pay later uses the amount extracted by amount extraction to
    // determine if the current purchase is eligible for buy now pay later.
    kBnpl = 0,
    kMaxValue = kBnpl,
  };

  explicit AmountExtractionManager(BrowserAutofillManager* autofill_manager);
  AmountExtractionManager(const AmountExtractionManager& other) = delete;
  AmountExtractionManager& operator=(const AmountExtractionManager& other) =
      delete;
  virtual ~AmountExtractionManager();

  // Timeout limit for the regex-base amount extraction in millisecond.
  static constexpr base::TimeDelta kAmountExtractionWaitTime =
      base::Milliseconds(150);

  // Timeout limit for the ai-based amount extraction in millisecond.
  static constexpr base::TimeDelta kAiBasedAmountExtractionWaitTime =
      base::Seconds(5);

  // This function attempts to convert a string representation of a monetary
  // value in dollars into a int64_t by parsing it as a double and multiplying
  // the result by 1,000,000. It assumes the input uses a decimal point ('.') as
  // the separator for fractional values (not a decimal comma). The function
  // only supports English-style monetary representations like $, USD, etc.
  // Multiplication by 1,000,000 is done to represent the monetary value in
  // micro-units (1 dollar = 1,000,000 micro-units), which is commonly used in
  // systems that require high precision for financial calculations.
  static std::optional<int64_t> MaybeParseAmountToMonetaryMicroUnits(
      const std::string& amount);

  // Validates the AmountExtractionResponse returned from the server-side AI.
  // A valid response should be with a non-negative value for the field of
  // `final_checkout_amount` and the field of `currency` should be "USD".
  AiAmountExtractionResult::ResultType ValidateAmountExtractionResponse(
      const optimization_guide::proto::AmountExtractionResponse& response);

  // Returns the set of all eligible features that depend on amount extraction
  // result when:
  //   Autofill Payment Methods are enabled for the user;
  //   Autofill provides non-empty, non-suppressed suggestions;
  //   The form being interacted with is a credit card form but not the CVC
  //   field of the credit card form;
  //   There is a feature that can use amount extraction on the current
  //   checkout page;
  //   Amount Extraction feature is enabled;
  //   In the AI-based amount extraction case, if a BNPL suggestion is present;
  virtual DenseSet<EligibleFeature> GetEligibleFeatures(
      bool is_autofill_payments_enabled,
      bool should_suppress_suggestions,
      const std::vector<Suggestion>& suggestions,
      FillingProduct filling_product,
      FieldType field_type) const;

  // Fetch the page content for the AI-based amount extraction.
  virtual void FetchAiPageContent();

  // Callback function for `AutofillClient::GetAiPageContent`.
  virtual void OnAiPageContentReceived(
      std::optional<optimization_guide::proto::AnnotatedPageContent> result);

  // Trigger the search for the final checkout amount from the DOM of the
  // current page.
  virtual void TriggerCheckoutAmountExtraction();

  // Trigger the search for the final checkout amount using server-side AI.
  virtual void TriggerCheckoutAmountExtractionWithAi();

  // Indicates whether the AI-based amount extraction timed out for the current
  // page load. Tied to the lifecycle of `this` and should not be reset by
  // `Reset()`.
  bool HasTimedOutForPageLoad() const;

  // Indicates whether the AI-based amount extraction has found an unsupported
  // currency. Tied to the lifecycle of `this` and should not be reset by
  // `Reset()`.
  bool SeenUnsupportedCurrencyForPageLoad() const;

 private:
  friend class AmountExtractionManagerTest;
  friend class AmountExtractionManagerTestApi;
  friend class BnplManager;

  // Invoked after the amount extraction process completes.
  // `extracted_amount` provides the extracted amount upon success and an
  // empty string upon failure. `search_request_start_timestamp` is the time
  // when TriggerCheckoutAmountExtraction is called.
  virtual void OnCheckoutAmountReceived(
      base::TimeTicks search_request_start_timestamp,
      const std::string& extracted_amount);

  // Invoked once the amount extraction from the model executor is complete.
  virtual void OnCheckoutAmountReceivedFromAi(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Checks whether the current amount search has reached the timeout or not.
  // If so, cancel the ongoing search.
  virtual void OnTimeoutReached();

  // Checks eligibility of features depending on amount extraction result, and
  // returns the eligible features.
  DenseSet<EligibleFeature>
  CheckEligibilityForFeaturesRequiringAmountExtraction() const;

  // Gets the driver associated with the main frame as the final checkout
  // amount is on the main frame.
  AutofillDriver* GetMainFrameDriver();

  // Cancels in-progress requests and resets the state. Also invalidates
  // `AmountExtractionManager` weak pointers from the factory.
  void Reset();

  // Logs the result of the AI-based amount extraction, but only if a result
  // has not been logged already.
  void LogAiAmountExtractionResultIfApplicable(
      AiAmountExtractionResult::ResultType result,
      std::optional<base::TimeDelta> latency);

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> autofill_manager_;

  // Once it is set, it can not be reset, as it should be set for the
  // lifetime of `this`. This ensures the amount extraction result metric is
  // logged once per page load.
  bool has_logged_amount_extraction_result_ = false;

  // Set to true after the first time the annotated page content (APC) fetch
  // result is logged. Ensures that logging occurs at most once per page load.
  bool has_logged_apc_fetch_result_ = false;

  // Indicates whether there is an amount search ongoing or not. If set, do not
  // trigger the search. It gets reset to false once the search is done. This is
  // to avoid re-triggering amount extraction multiple times during an ongoing
  // search.
  bool search_request_pending_ = false;

  // The timer to enforce the timeout on client-side for AI-based amount
  // extraction.
  base::OneShotTimer timeout_timer_;

  // Aggregated status for AI-based amount extraction for the current page load.
  // Tied to the lifecycle of `this` and should not be reset by `Reset()`.
  AmountExtractionStatus amount_extraction_status_;

  // The time when the AI-based amount extraction was initiated. This is used to
  // calculate the latency of amount extraction process, which measured from
  // when the page content is started to fetch until the amount is received from
  // AI model. It is reset after the latency is collected or when the page is
  // refreshed.
  std::optional<base::TimeTicks> ai_amount_extraction_start_time_;

  base::WeakPtrFactory<AmountExtractionManager> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_
