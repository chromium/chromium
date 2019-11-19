// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "base/bind.h"
#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"
#include "chrome/test/payments/android/payment_request_test_bridge.h"

namespace payments {

PaymentRequestTestController::PaymentRequestTestController() {}

PaymentRequestTestController::~PaymentRequestTestController() = default;

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
      base::BindRepeating(&PaymentRequestTestController::OnShowAppsReady,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnNotSupportedError,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnConnectionTerminated,
                          base::Unretained(this)),
      base::BindRepeating(&PaymentRequestTestController::OnAbortCalled,
                          base::Unretained(this)));

  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_incognito_, valid_ssl_,
      /*is_browser_window_active=*/true, can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false);
}

void PaymentRequestTestController::SetObserver(
    PaymentRequestTestObserver* observer) {
  observer_ = observer;
}

void PaymentRequestTestController::SetIncognito(bool is_incognito) {
  is_incognito_ = is_incognito;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_incognito_, valid_ssl_,
      /*is_browser_window_active=*/true, can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false);
}

void PaymentRequestTestController::SetValidSsl(bool valid_ssl) {
  valid_ssl_ = valid_ssl;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_incognito_, valid_ssl_,
      /*is_browser_window_active=*/true, can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false);
}

void PaymentRequestTestController::SetCanMakePaymentEnabledPref(
    bool can_make_payment_enabled) {
  can_make_payment_pref_ = can_make_payment_enabled;
  SetUseDelegateOnPaymentRequestForTesting(
      /*use_delegate_for_test=*/true, is_incognito_, valid_ssl_,
      /*is_browser_window_active=*/true, can_make_payment_pref_,
      /*skip_ui_for_basic_card=*/false);
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

void PaymentRequestTestController::OnShowAppsReady() {
  if (observer_)
    observer_->OnShowAppsReady();
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

}  // namespace payments
