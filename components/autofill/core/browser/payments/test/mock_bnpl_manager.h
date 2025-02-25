// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_

#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockBnplManager : public payments::BnplManager {
 public:
  explicit MockBnplManager(TestAutofillClient* test_autofill_client);
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
              (const std::optional<uint64_t>&),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_BNPL_MANAGER_H_
