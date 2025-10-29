// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_

#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class TestBrowserAutofillManager;

class MockBnplManager : public payments::BnplManager {
 public:
  explicit MockBnplManager(
      TestBrowserAutofillManager* test_browser_autofill_manager);
  ~MockBnplManager() override;

  MOCK_METHOD(void,
              NotifyOfSuggestionGeneration,
              (const AutofillSuggestionTriggerSource),
              (override));

  MOCK_METHOD(void,
              OnSuggestionsShown,
              (base::span<const Suggestion>,
               payments::UpdateSuggestionsCallback),
              (override));

  MOCK_METHOD(void,
              OnAmountExtractionReturned,
              (const std::optional<int64_t>&, bool),
              (override));

  MOCK_METHOD(void,
              OnAmountExtractionReturnedFromAi,
              (const std::optional<int64_t>&, bool),
              (override));

  MOCK_METHOD(void,
              OnDidAcceptBnplSuggestion,
              (std::optional<int64_t> final_checkout_amount,
               OnBnplVcnFetchedCallback on_bnpl_vcn_fetched_callback),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_
