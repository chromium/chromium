// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_MOCK_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CORE_MOCK_PAYMENT_REQUEST_DELEGATE_H_

#include "components/payments/core/payment_request_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace payments {

class MockPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  MockPaymentRequestDelegate();
  ~MockPaymentRequestDelegate() override;
  MOCK_METHOD1(ShowDialog, void(PaymentRequest* request));
  MOCK_METHOD0(RetryDialog, void());
  MOCK_METHOD0(CloseDialog, void());
  MOCK_METHOD0(ShowErrorMessage, void());
  MOCK_METHOD0(ShowProcessingSpinner, void());
  MOCK_CONST_METHOD0(IsBrowserWindowActive, bool());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_CONST_METHOD0(GetApplicationLocale, const std::string&());
  MOCK_CONST_METHOD0(IsIncognito, bool());
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());
  MOCK_METHOD2(
      DoFullCardRequest,
      void(const autofill::CreditCard& credit_card,
           base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
               result_delegate));
  MOCK_METHOD0(GetAddressNormalizer, autofill::AddressNormalizer*());
  MOCK_METHOD0(GetRegionDataLoader, autofill::RegionDataLoader*());
  MOCK_METHOD0(GetUkmRecorder, ukm::UkmRecorder*());
  MOCK_CONST_METHOD0(GetAuthenticatedEmail, std::string());
  MOCK_METHOD0(GetPrefService, PrefService*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPaymentRequestDelegate);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_MOCK_PAYMENT_REQUEST_DELEGATE_H_
