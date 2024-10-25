// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_CLIENT_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_CLIENT_H_

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class PaymentsDataManager;
}  // namespace autofill

namespace payments::facilitated {

class FacilitatedPaymentsNetworkInterface;

// A mock for the facilitated payment "client" interface.
class MockFacilitatedPaymentsClient : public FacilitatedPaymentsClient {
 public:
  MockFacilitatedPaymentsClient();
  ~MockFacilitatedPaymentsClient() override;

  MOCK_METHOD(void,
              LoadRiskData,
              (base::OnceCallback<void(const std::string&)>),
              (override));
  MOCK_METHOD(autofill::PaymentsDataManager*,
              GetPaymentsDataManager,
              (),
              (override));
  MOCK_METHOD(FacilitatedPaymentsNetworkInterface*,
              GetFacilitatedPaymentsNetworkInterface,
              (),
              (override));
  MOCK_METHOD(std::optional<CoreAccountInfo>,
              GetCoreAccountInfo,
              (),
              (override));
  MOCK_METHOD(bool, IsInLandscapeMode, (), (override));
  MOCK_METHOD(void,
              ShowPixPaymentPrompt,
              (base::span<const autofill::BankAccount> pix_account_suggestions,
               base::OnceCallback<void(bool, int64_t)>),
              (override));
  MOCK_METHOD(void,
              ShowEwalletPaymentPrompt,
              (base::span<const autofill::Ewallet> ewallet_suggestions,
               base::OnceCallback<void(bool, int64_t)>),
              (override));
  MOCK_METHOD(void, ShowProgressScreen, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, DismissPrompt, (), (override));
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MOCK_FACILITATED_PAYMENTS_CLIENT_H_
