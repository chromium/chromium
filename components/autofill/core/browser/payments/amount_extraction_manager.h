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
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/optimization_guide/proto/features/amount_extraction.pb.h"

namespace autofill {
class AutofillDriver;
class BrowserAutofillManager;
}  // namespace autofill

namespace optimization_guide {

namespace proto {
class AnnotatedPageContent;
}  // namespace proto

class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace autofill::payments {

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
      base::Seconds(10);

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
  // `final_checkout_amount` and the field of `currency` should be from the
  // standard ISO 4217 currency code.
  bool IsValidAmountExtractionResponse(
      const AmountExtractionResponse& response);

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

 private:
  friend class AmountExtractionManagerTest;
  friend class AmountExtractionManagerTestApi;

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

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> autofill_manager_;

  // If true, the metrics for the amount extraction result was already logged
  // and should not log again.
  bool has_logged_amount_extraction_result_ = false;

  // Indicates whether there is an amount search ongoing or not. If set, do not
  // trigger the search. It gets reset to false once the search is done. This is
  // to avoid re-triggering amount extraction multiple times during an ongoing
  // search.
  bool search_request_pending_ = false;

  // Member variable to store the fetched page content temporarily. This data is
  // generated when credit card form is clicked and BNPL feature is available
  // for this profile. It is about 10Kb in size depending on the merchant
  // checkout page.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      ai_page_content_;

  // Flag to indicate if an AI page content fetch is in progress. If set, do not
  // trigger the next request to generate the page content. This is to avoid
  // multiple page content requests when a user quickly clicks on the payment
  // form multiple times or by scripts.
  bool is_fetching_ai_page_content_ = false;

  base::WeakPtrFactory<AmountExtractionManager> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_MANAGER_H_
