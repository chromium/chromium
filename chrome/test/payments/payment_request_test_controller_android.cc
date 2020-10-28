// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "base/bind.h"
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

bool PaymentRequestTestController::ConfirmPayment() {
  NOTIMPLEMENTED();
  return false;
}

bool PaymentRequestTestController::ConfirmMinimalUI() {
  return ConfirmMinimalUIForTest();
}

bool PaymentRequestTestController::DismissMinimalUI() {
  return DismissMinimalUIForTest();
}

bool PaymentRequestTestController::IsAndroidMarshmallowOrLollipop() {
  return IsAndroidMarshmallowOrLollipopForTest();
}

void PaymentRequestTestController::SetUpOnMainThread() {
  ChromeBackgroundTaskFactory::SetAsDefault();

  // Register |this| as the observer for future PaymentRequests created in
  // Java.
  SetUseNativeObserverOnPaymentRequestForTesting(
      base::BindRepeating(&PaymentRequestTestController::OnCanMakePaymentCalled,
                          base::Unretained(this)),
      base::BindRepeating(
          &PaymentRequestTestController::OnCanMakePaymentReturned,
          base::Unretained(this)),
      base::BindRepeating(
          &PaymentRequestTestController::OnHasEnrolledInstrumentCalled,
          base::Unretained(this)),
      base::BindRepeating(
          &PaymentRequestTestController::OnHasEnrolledInstrumentReturned,
          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnAppListReady,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::set_app_descriptions,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnNotSupportedError,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnConnectionTerminated,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnAbortCalled,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnCompleteCalled,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnMinimalUIReady,
                          base::Unretained(this)));

  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_off_the_record_, valid_ssl_,
      can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false, twa_package_name_);
}

void PaymentRequestTestController::SetObserver(
    PaymentRequestTestObserver* observer) {
  observer_ = observer;
}

void PaymentRequestTestController::SetOffTheRecord(bool is_off_the_record) {
  is_off_the_record_ = is_off_the_record;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_off_the_record_, valid_ssl_,
      can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false, twa_package_name_);
}

void PaymentRequestTestController::SetValidSsl(bool valid_ssl) {
  valid_ssl_ = valid_ssl;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_off_the_record_, valid_ssl_,
      can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false, twa_package_name_);
}

void PaymentRequestTestController::SetCanMakePaymentEnabledPref(
    bool can_make_payment_enabled) {
  can_make_payment_pref_ = can_make_payment_enabled;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_off_the_record_, valid_ssl_,
      can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false, twa_package_name_);
}

void PaymentRequestTestController::SetTwaPackageName(
    const std::string& twa_package_name) {
  twa_package_name_ = twa_package_name;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_off_the_record_, valid_ssl_,
      can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false, twa_package_name_);
}

void PaymentRequestTestController::SetHasAuthenticator(bool has_authenticator) {
  // TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for
  // Android, so secure payment confirmation can be integration tested on
  // Android as well.
  has_authenticator_ = has_authenticator;
}

void PaymentRequestTestController::SetTwaPaymentApp(
    const std::string& method_name,
    const std::string& response) {
  // Intentionally left blank.
}

void PaymentRequestTestController::OnCanMakePaymentCalled() {
  if (observer_)
    observer_->OnCanMakePaymentCalled();
}

void PaymentRequestTestController::OnCanMakePaymentReturned() {
  if (observer_)
    observer_->OnCanMakePaymentReturned();
}

void PaymentRequestTestController::OnHasEnrolledInstrumentCalled() {
  if (observer_)
    observer_->OnHasEnrolledInstrumentCalled();
}

void PaymentRequestTestController::OnHasEnrolledInstrumentReturned() {
  if (observer_)
    observer_->OnHasEnrolledInstrumentReturned();
}

void PaymentRequestTestController::OnAppListReady() {
  if (observer_)
    observer_->OnAppListReady();
}
void PaymentRequestTestController::OnNotSupportedError() {
  if (observer_)
    observer_->OnNotSupportedError();
}

void PaymentRequestTestController::OnConnectionTerminated() {
  if (observer_)
    observer_->OnConnectionTerminated();
}

void PaymentRequestTestController::OnAbortCalled() {
  if (observer_)
    observer_->OnAbortCalled();
}

void PaymentRequestTestController::OnCompleteCalled() {
  if (observer_)
    observer_->OnCompleteCalled();
}

void PaymentRequestTestController::OnMinimalUIReady() {
  if (observer_)
    observer_->OnMinimalUIReady();
}

}  // namespace payments
