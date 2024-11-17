// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/test_payment_request_delegate.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace payments {

TestPaymentRequestDelegate::TestPaymentRequestDelegate(
    std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
    autofill::PersonalDataManager* personal_data_manager)
    : main_task_executor_(std::move(task_executor)),
      personal_data_manager_(personal_data_manager),
      locale_("en-US"),
      last_committed_url_("https://shop.com"),
      test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      payments_network_interface_(
          test_shared_loader_factory_,
          /*identity_manager=*/nullptr,
          &personal_data_manager->payments_data_manager()),
      full_card_request_(&autofill_client_,
                         &payments_network_interface_,
                         personal_data_manager) {}

TestPaymentRequestDelegate::~TestPaymentRequestDelegate() = default;

autofill::PersonalDataManager*
TestPaymentRequestDelegate::GetPersonalDataManager() {
  return personal_data_manager_;
}

const std::string& TestPaymentRequestDelegate::GetApplicationLocale() const {
  return locale_;
}

bool TestPaymentRequestDelegate::IsOffTheRecord() const {
  return false;
}

const GURL& TestPaymentRequestDelegate::GetLastCommittedURL() const {
  return last_committed_url_;
}

autofill::AddressNormalizer*
TestPaymentRequestDelegate::GetAddressNormalizer() {
  return &address_normalizer_;
}

autofill::RegionDataLoader* TestPaymentRequestDelegate::GetRegionDataLoader() {
  return nullptr;
}

ukm::UkmRecorder* TestPaymentRequestDelegate::GetUkmRecorder() {
  return nullptr;
}

autofill::TestAddressNormalizer*
TestPaymentRequestDelegate::test_address_normalizer() {
  return &address_normalizer_;
}

void TestPaymentRequestDelegate::DelayFullCardRequestCompletion() {
  instantaneous_full_card_request_result_ = false;
}

void TestPaymentRequestDelegate::CompleteFullCardRequest() {
  DCHECK(instantaneous_full_card_request_result_ == false);
  full_card_result_delegate_->OnFullCardRequestSucceeded(
      full_card_request_, full_card_request_card_, u"123");
}

std::string TestPaymentRequestDelegate::GetAuthenticatedEmail() const {
  return "";
}

PrefService* TestPaymentRequestDelegate::GetPrefService() {
  return nullptr;
}

bool TestPaymentRequestDelegate::IsBrowserWindowActive() const {
  return true;
}

}  // namespace payments
