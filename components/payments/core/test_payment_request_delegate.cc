// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/test_payment_request_delegate.h"

#include <utility>

#include "components/autofill/core/browser/data_manager/personal_data_manager.h"

namespace payments {

TestPaymentRequestDelegate::TestPaymentRequestDelegate(
    std::unique_ptr<base::SingleThreadTaskExecutor> task_executor,
    autofill::PersonalDataManager* personal_data_manager)
    : main_task_executor_(std::move(task_executor)),
      personal_data_manager_(personal_data_manager),
      locale_("en-US"),
      last_committed_url_("https://shop.com") {}

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


autofill::TestAddressNormalizer*
TestPaymentRequestDelegate::test_address_normalizer() {
  return &address_normalizer_;
}

PrefService* TestPaymentRequestDelegate::GetPrefService() {
  return nullptr;
}

bool TestPaymentRequestDelegate::IsBrowserWindowActive() const {
  return true;
}

}  // namespace payments
