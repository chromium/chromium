// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace payments {

class CanMakePaymentTestChromePaymentRequestDelegate
    : public ChromePaymentRequestDelegate {
 public:
  CanMakePaymentTestChromePaymentRequestDelegate(
      content::WebContents* web_contents,
      bool is_incognito,
      bool valid_ssl,
      PrefService* prefs)
      : ChromePaymentRequestDelegate(web_contents),
        is_incognito_(is_incognito),
        valid_ssl_(valid_ssl),
        prefs_(prefs) {}

  bool IsIncognito() const override { return is_incognito_; }
  std::string GetInvalidSslCertificateErrorMessage() override {
    return valid_ssl_ ? "" : "Invalid SSL certificate";
  }
  PrefService* GetPrefService() override { return prefs_; }
  bool IsBrowserWindowActive() const override { return true; }

 private:
  const bool is_incognito_;
  const bool valid_ssl_;
  PrefService* const prefs_;
};

class PaymentRequestTestController::ObserverConverter
    : public PaymentRequest::ObserverForTest {
 public:
  explicit ObserverConverter(PaymentRequestTestController* controller)
      : controller_(controller) {}

  void OnCanMakePaymentCalled() override {
    controller_->OnCanMakePaymentCalled();
  }
  void OnCanMakePaymentReturned() override {
    controller_->OnCanMakePaymentReturned();
  }
  void OnHasEnrolledInstrumentCalled() override {
    controller_->OnHasEnrolledInstrumentCalled();
  }
  void OnHasEnrolledInstrumentReturned() override {
    controller_->OnHasEnrolledInstrumentReturned();
  }
  void OnShowAppsReady() override { controller_->OnShowAppsReady(); }
  void OnNotSupportedError() override { controller_->OnNotSupportedError(); }
  void OnConnectionTerminated() override {
    controller_->OnConnectionTerminated();
  }
  void OnAbortCalled() override { controller_->OnAbortCalled(); }

 private:
  PaymentRequestTestController* const controller_;
};

PaymentRequestTestController::PaymentRequestTestController()
    : prefs_(std::make_unique<sync_preferences::TestingPrefServiceSyncable>()),
      observer_converter_(std::make_unique<ObserverConverter>(this)) {}

PaymentRequestTestController::~PaymentRequestTestController() = default;

void PaymentRequestTestController::SetUpOnMainThread() {
  // Register all prefs with our pref testing service, since we're not using the
  // one chrome sets up.
  payments::RegisterProfilePrefs(prefs_->registry());

  UpdateDelegateFactory();
}

void PaymentRequestTestController::SetObserver(
    PaymentRequestTestObserver* observer) {
  observer_ = observer;
}

void PaymentRequestTestController::SetIncognito(bool is_incognito) {
  is_incognito_ = is_incognito;
  UpdateDelegateFactory();
}

void PaymentRequestTestController::SetValidSsl(bool valid_ssl) {
  valid_ssl_ = valid_ssl;
  UpdateDelegateFactory();
}

void PaymentRequestTestController::SetCanMakePaymentEnabledPref(
    bool can_make_payment_enabled) {
  can_make_payment_pref_ = can_make_payment_enabled;
  prefs_->SetBoolean(kCanMakePaymentEnabled, can_make_payment_pref_);
  UpdateDelegateFactory();
}

void PaymentRequestTestController::UpdateDelegateFactory() {
  SetPaymentRequestFactoryForTesting(base::BindRepeating(
      [](PaymentRequest::ObserverForTest* observer_for_test, bool is_incognito,
         bool valid_ssl, PrefService* prefs,
         mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
         content::RenderFrameHost* render_frame_host) {
        content::WebContents* web_contents =
            content::WebContents::FromRenderFrameHost(render_frame_host);
        DCHECK(web_contents);
        auto delegate =
            std::make_unique<CanMakePaymentTestChromePaymentRequestDelegate>(
                web_contents, is_incognito, valid_ssl, prefs);
        PaymentRequestWebContentsManager* manager =
            PaymentRequestWebContentsManager::GetOrCreateForWebContents(
                web_contents);
        manager->CreatePaymentRequest(web_contents->GetMainFrame(),
                                      web_contents, std::move(delegate),
                                      std::move(receiver), observer_for_test);
      },
      observer_converter_.get(), is_incognito_, valid_ssl_, prefs_.get()));
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
