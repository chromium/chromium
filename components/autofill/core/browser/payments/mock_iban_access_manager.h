// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_IBAN_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_IBAN_ACCESS_MANAGER_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockIbanAccessManager : public IbanAccessManager {
 public:
  explicit MockIbanAccessManager(AutofillClient* client);
  MockIbanAccessManager(const MockIbanAccessManager&) = delete;
  MockIbanAccessManager& operator=(const MockIbanAccessManager&) = delete;
  ~MockIbanAccessManager() override;

  MOCK_METHOD(void,
              FetchValue,
              (const Suggestion::BackendId&,
               (base::OnceCallback<void(const std::u16string& value)>)),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MOCK_IBAN_ACCESS_MANAGER_H_
