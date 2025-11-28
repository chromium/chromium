// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_OPTIMIZATION_GUIDE_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_OPTIMIZATION_GUIDE_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillOptimizationGuideDecider
    : public AutofillOptimizationGuideDecider {
 public:
  MockAutofillOptimizationGuideDecider();
  ~MockAutofillOptimizationGuideDecider() override;

  MOCK_METHOD(void,
              OnDidParseForm,
              (const FormStructure&, const PaymentsDataManager&),
              (override));
  MOCK_METHOD(CreditCardCategoryBenefit::BenefitCategory,
              AttemptToGetEligibleCreditCardBenefitCategory,
              (std::string_view issuer_id, const GURL& url),
              (const override));
  MOCK_METHOD(bool,
              ShouldBlockSingleFieldSuggestions,
              (const GURL&, const AutofillField*),
              (const override));
  MOCK_METHOD(bool,
              ShouldBlockFormFieldSuggestion,
              (const GURL&, const CreditCard&),
              (const override));
  MOCK_METHOD(bool,
              ShouldBlockFlatRateBenefitSuggestionLabelsForUrl,
              (const GURL& url),
              (const override));
  MOCK_METHOD(bool,
              IsUrlEligibleForBnplIssuer,
              (BnplIssuer::IssuerId issuer_id, const GURL& url),
              (const override));
  MOCK_METHOD(void,
              OnPaymentsDataLoaded,
              (const PaymentsDataManager& payments_data_manager),
              (override));
  MOCK_METHOD(bool,
              IsIframeUrlAllowlistedForActor,
              (const GURL&),
              (const override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_OPTIMIZATION_GUIDE_MOCK_AUTOFILL_OPTIMIZATION_GUIDE_DECIDER_H_
