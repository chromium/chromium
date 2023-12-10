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
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/payments/core/payment_request_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

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
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsOffTheRecord() const override;
  const GURL& GetLastCommittedURL() const override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;

  autofill::TestAddressNormalizer* test_address_normalizer();
  void DelayFullCardRequestCompletion();
  void CompleteFullCardRequest();

 private:
  std::unique_ptr<base::SingleThreadTaskExecutor> main_task_executor_;
  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  std::string locale_;
  const GURL last_committed_url_;
  autofill::TestAddressNormalizer address_normalizer_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  autofill::TestAutofillClient autofill_client_;
  autofill::payments::PaymentsNetworkInterface payments_network_interface_;
  autofill::payments::FullCardRequest full_card_request_;

  bool instantaneous_full_card_request_result_ = true;
  autofill::CreditCard full_card_request_card_;
  base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
      full_card_result_delegate_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_
