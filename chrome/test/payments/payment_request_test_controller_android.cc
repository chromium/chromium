// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"
#include "chrome/test/payments/android/payment_request_test_bridge.h"

namespace payments {

PaymentRequestTestController::PaymentRequestTestController() = default;

PaymentRequestTestController::~PaymentRequestTestController() = default;

content::WebContents*
PaymentRequestTestController::GetPaymentHandlerWebContents() {
  return GetPaymentHandlerWebContentsForTest();
}

bool PaymentRequestTestController::ClickPaymentHandlerSecurityIcon() {
  return ClickPaymentHandlerSecurityIconForTest();
}

bool PaymentRequestTestController::ClickPaymentHandlerCloseButton() {
  return ClickPaymentHandlerCloseButtonForTest();
}

bool PaymentRequestTestController::CloseDialog() {
  return CloseDialogForTest();
}

bool PaymentRequestTestController::ConfirmPayment() {
  NOTIMPLEMENTED();
  return false;
}

bool PaymentRequestTestController::ClickOptOut() {
  return ClickSecurePaymentConfirmationOptOutForTest();
}

void PaymentRequestTestController::SetUpOnMainThread() {
  ChromeBackgroundTaskFactory::SetAsDefault();

  // Register |this| as the observer for future PaymentRequests created in
  // Java.
  SetUseNativeObserverOnPaymentRequestForTesting(
      base::BindRepeating(&PaymentRequestTestController::OnCanMakePaymentCalled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PaymentRequestTestController::OnCanMakePaymentReturned,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PaymentRequestTestController::OnHasEnrolledInstrumentCalled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PaymentRequestTestController::OnHasEnrolledInstrumentReturned,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnAppListReady,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::set_app_descriptions,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PaymentRequestTestController::set_shipping_section_visible,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &PaymentRequestTestController::set_contact_section_visible,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnErrorDisplayed,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnNotSupportedError,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnConnectionTerminated,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnAbortCalled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnCompleteCalled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PaymentRequestTestController::OnUIDisplayed,
                          weak_ptr_factory_.GetWeakPtr()));

  SetUseDelegateOnPaymentRequestForTesting(is_off_the_record_, valid_ssl_,
                                           can_make_payment_pref_,
                                           twa_package_name_);
}

void PaymentRequestTestController::SetObserver(
    PaymentRequestTestObserver* observer) {
  observer_ = observer;
}

void PaymentRequestTestController::SetOffTheRecord(bool is_off_the_record) {
  is_off_the_record_ = is_off_the_record;
  SetUseDelegateOnPaymentRequestForTesting(is_off_the_record_, valid_ssl_,
                                           can_make_payment_pref_,
                                           twa_package_name_);
}

void PaymentRequestTestController::SetValidSsl(bool valid_ssl) {
  valid_ssl_ = valid_ssl;
  SetUseDelegateOnPaymentRequestForTesting(is_off_the_record_, valid_ssl_,
                                           can_make_payment_pref_,
                                           twa_package_name_);
}

void PaymentRequestTestController::SetCanMakePaymentEnabledPref(
    bool can_make_payment_enabled) {
  can_make_payment_pref_ = can_make_payment_enabled;
  SetUseDelegateOnPaymentRequestForTesting(is_off_the_record_, valid_ssl_,
                                           can_make_payment_pref_,
                                           twa_package_name_);
}

void PaymentRequestTestController::SetTwaPackageName(
    const std::string& twa_package_name) {
  twa_package_name_ = twa_package_name;
  SetUseDelegateOnPaymentRequestForTesting(is_off_the_record_, valid_ssl_,
                                           can_make_payment_pref_,
                                           twa_package_name_);
}

void PaymentRequestTestController::SetHasAuthenticator(bool has_authenticator) {
  has_authenticator_ = has_authenticator;
}

void PaymentRequestTestController::SetTwaPaymentApp(
    const std::string& method_name,
    const std::string& response) {
  // Intentionally left blank.
}

void PaymentRequestTestController::OnCanMakePaymentCalled() {
  if (observer_) {
    observer_->OnCanMakePaymentCalled();
  }
}

void PaymentRequestTestController::OnCanMakePaymentReturned() {
  if (observer_) {
    observer_->OnCanMakePaymentReturned();
  }
}

void PaymentRequestTestController::OnHasEnrolledInstrumentCalled() {
  if (observer_) {
    observer_->OnHasEnrolledInstrumentCalled();
  }
}

void PaymentRequestTestController::OnHasEnrolledInstrumentReturned() {
  if (observer_) {
    observer_->OnHasEnrolledInstrumentReturned();
  }
}

void PaymentRequestTestController::OnAppListReady() {
  if (observer_) {
    observer_->OnAppListReady();
  }
}
void PaymentRequestTestController::OnErrorDisplayed() {
  if (observer_) {
    observer_->OnErrorDisplayed();
  }
}
void PaymentRequestTestController::OnNotSupportedError() {
  if (observer_) {
    observer_->OnNotSupportedError();
  }
}

void PaymentRequestTestController::OnConnectionTerminated() {
  if (observer_) {
    observer_->OnConnectionTerminated();
  }
}

void PaymentRequestTestController::OnAbortCalled() {
  if (observer_) {
    observer_->OnAbortCalled();
  }
}

void PaymentRequestTestController::OnCompleteCalled() {
  if (observer_) {
    observer_->OnCompleteCalled();
  }
}

void PaymentRequestTestController::OnUIDisplayed() {
  if (observer_) {
    observer_->OnUIDisplayed();
  }
}

}  // namespace payments
