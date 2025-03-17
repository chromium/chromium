// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"

namespace autofill {

MockBnplManager::MockBnplManager(TestAutofillClient* test_autofill_client)
    : BnplManager(test_autofill_client) {
  ON_CALL(*this, NotifyOfSuggestionGeneration)
      .WillByDefault(
          [this](const AutofillSuggestionTriggerSource trigger_source) {
            BnplManager::NotifyOfSuggestionGeneration(trigger_source);
          });
}

MockBnplManager::~MockBnplManager() = default;

}  // namespace autofill
