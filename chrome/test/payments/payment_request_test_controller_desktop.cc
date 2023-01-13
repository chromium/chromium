// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "base/check.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/core/payment_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace payments {

class PaymentRequestTestController::ObserverConverter
    : public PaymentRequest::ObserverForTest,
      public PaymentUIObserver {
 public:
  explicit ObserverConverter(PaymentRequestTestController* controller)
      : controller_(controller) {}

  // PaymentRequest::ObserverForTest:
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
  void OnAppListReady(base::WeakPtr<PaymentRequest> payment_request) override {
    DCHECK(payment_request);
    std::vector<AppDescription> descriptions(
        payment_request->state()->available_apps().size());
    size_t i = 0;
    for (const auto& app : payment_request->state()->available_apps()) {
      auto* description = &descriptions[i++];
      description->label = base::UTF16ToUTF8(app->GetLabel());
      description->sublabel = base::UTF16ToUTF8(app->GetSublabel());
      base::WeakPtr<PaymentRequestSpec> spec = payment_request->spec();
      const auto& total = spec->GetTotal(app.get());
      description->total = total->amount->currency + " " + total->amount->value;
    }
    controller_->set_app_descriptions(descriptions);
    controller_->set_shipping_section_visible(
        payment_request->state()->ShouldShowShippingSection());
    controller_->set_contact_section_visible(
        payment_request->state()->ShouldShowContactSection());

    controller_->OnAppListReady();
  }
  void OnErrorDisplayed() override { controller_->OnErrorDisplayed(); }
  void OnNotSupportedError() override { controller_->OnNotSupportedError(); }
  void OnConnectionTerminated() override {
    controller_->OnConnectionTerminated();
  }
  void OnPayCalled() override {}
  void OnAbortCalled() override { controller_->OnAbortCalled(); }
  void OnCompleteCalled() override { controller_->OnCompleteCalled(); }

  // PaymentUIObserver:
  void OnUIDisplayed() const override { controller_->OnUIDisplayed(); }

  base::WeakPtr<ObserverConverter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<PaymentRequestTestController> controller_;

  base::WeakPtrFactory<ObserverConverter> weak_ptr_factory_{this};
};

PaymentRequestTestController::PaymentRequestTestController()
    : prefs_(std::make_unique<sync_preferences::TestingPrefServiceSyncable>()),
      observer_converter_(std::make_unique<ObserverConverter>(this)) {}

PaymentRequestTestController::~PaymentRequestTestController() = default;

content::WebContents*
PaymentRequestTestController::GetPaymentHandlerWebContents() {
  // Todo(1053722): return the invoked payment app's web contents for testing.
  return nullptr;
}

bool PaymentRequestTestController::ConfirmPayment() {
  if (!delegate_) {
    return false;
  }

  PaymentRequestDialog* dialog = delegate_->GetDialogForTesting();
  if (!dialog) {
    return false;
  }

  dialog->ConfirmPaymentForTesting();
  return true;
}

bool PaymentRequestTestController::ClickOptOut() {
  if (!delegate_) {
    return false;
  }

  PaymentRequestDialog* dialog = delegate_->GetDialogForTesting();
  SecurePaymentConfirmationNoCreds* no_creds_dialog =
      delegate_->GetNoMatchingCredentialsDialogForTesting();
  if (!dialog && !no_creds_dialog) {
    return false;
  }

  // The SPC dialog will exist, but will not be showing a view, when the
  // no-matching-creds dialog is present. Therefore, we have to check the
  // no-matching-creds case first, as it will only be present when it is showing
  // a view.
  if (no_creds_dialog) {
    return no_creds_dialog->ClickOptOutForTesting();
  }
  return dialog->ClickOptOutForTesting();
}

bool PaymentRequestTestController::ClickPaymentHandlerCloseButton() {
  return CloseDialog();
}

bool PaymentRequestTestController::CloseDialog() {
  if (!delegate_) {
    return false;
  }

  PaymentRequestDialog* dialog = delegate_->GetDialogForTesting();
  SecurePaymentConfirmationNoCreds* no_creds_dialog =
      delegate_->GetNoMatchingCredentialsDialogForTesting();
  if (!dialog && !no_creds_dialog) {
    return false;
  }

  if (dialog) {
    dialog->CloseDialog();
  }

  if (no_creds_dialog) {
    no_creds_dialog->CloseDialog();
  }

  return true;
}

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

void PaymentRequestTestController::SetOffTheRecord(bool is_off_the_record) {
  is_off_the_record_ = is_off_the_record;
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

void PaymentRequestTestController::SetTwaPackageName(
    const std::string& twa_package_name) {
  twa_package_name_ = twa_package_name;
  UpdateDelegateFactory();
}

void PaymentRequestTestController::SetHasAuthenticator(bool has_authenticator) {
  has_authenticator_ = has_authenticator;
  UpdateDelegateFactory();
}

void PaymentRequestTestController::SetTwaPaymentApp(
    const std::string& method_name,
    const std::string& response) {
  twa_payment_app_method_name_ = method_name;
  twa_payment_app_response_ = response;
  UpdateDelegateFactory();
}

void PaymentRequestTestController::UpdateDelegateFactory() {
  SetPaymentRequestFactoryForTesting(base::BindRepeating(
      [](base::WeakPtr<ObserverConverter> observer_for_test,
         bool is_off_the_record, bool valid_ssl, PrefService* prefs,
         const std::string& twa_package_name, bool has_authenticator,
         const std::string& twa_payment_app_method_name,
         const std::string& twa_payment_app_response,
         base::WeakPtr<ContentPaymentRequestDelegate>* delegate_weakptr,
         mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
         content::RenderFrameHost* render_frame_host) {
        DCHECK(render_frame_host);
        DCHECK(render_frame_host->IsActive());

        auto delegate = std::make_unique<TestChromePaymentRequestDelegate>(
            render_frame_host);
        delegate->OverrideBrowserWindowActive(true);
        delegate->OverrideOffTheRecord(is_off_the_record);
        delegate->OverrideValidSSL(valid_ssl);
        delegate->OverridePrefService(prefs);
        delegate->set_twa_package_name(twa_package_name);
        delegate->set_has_authenticator(has_authenticator);
        delegate->set_payment_ui_observer(observer_for_test);
        *delegate_weakptr = delegate->GetContentWeakPtr();

        if (!twa_payment_app_method_name.empty()) {
          AndroidAppCommunication::GetForBrowserContext(
              render_frame_host->GetBrowserContext())
              ->SetAppForTesting(twa_package_name, twa_payment_app_method_name,
                                 twa_payment_app_response);
        }

        // PaymentRequest is a DocumentService, whose lifetime is managed by the
        // RenderFrameHost passed into the delegate.
        auto* request =
            new PaymentRequest(std::move(delegate), std::move(receiver));
        request->set_observer_for_test(observer_for_test);
      },
      observer_converter_->GetWeakPtr(), is_off_the_record_, valid_ssl_,
      prefs_.get(), twa_package_name_, has_authenticator_,
      twa_payment_app_method_name_, twa_payment_app_response_, &delegate_));
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

}  // namespace payments
