// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/data_quality/addresses/test_address_normalizer.h"
#include "components/payments/core/payment_request_delegate.h"
#include "url/gurl.h"

namespace payments {

class TestPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  TestPaymentRequestDelegate(
      std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
      autofill::PersonalDataManager* personal_data_manager);

  TestPaymentRequestDelegate(const TestPaymentRequestDelegate&) = delete;
  TestPaymentRequestDelegate& operator=(const TestPaymentRequestDelegate&) =
      delete;

  ~TestPaymentRequestDelegate() override;

  // PaymentRequestDelegate
  void ShowDialog(base::WeakPtr<PaymentRequest> request) override {}
  void RetryDialog() override {}
  void CloseDialog() override {}
  void ShowErrorMessage() override {}
  void ShowProcessingSpinner() override {}
  void ShowLoadingView() override {}
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsOffTheRecord() const override;
  const GURL& GetLastCommittedURL() const override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;

  autofill::TestAddressNormalizer* test_address_normalizer();

 private:
  std::unique_ptr<base::SingleThreadTaskExecutor> main_task_executor_;
  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  std::string locale_;
  const GURL last_committed_url_;
  autofill::TestAddressNormalizer address_normalizer_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_
