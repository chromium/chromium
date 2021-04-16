// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_test_controller.h"

#include "base/check.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#include "components/payments/content/android_app_communication.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/payments/content/payment_ui_observer.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_request_delegate.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace payments {
namespace {

class TestAuthenticator : public content::InternalAuthenticatorImpl {
 public:
  explicit TestAuthenticator(content::RenderFrameHost* rfh,
                             bool has_authenticator)
      : content::InternalAuthenticatorImpl(rfh),
        has_authenticator_(has_authenticator) {}

  ~TestAuthenticator() override = default;

  // autofill::InternalAuthenticator
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override {
    std::move(callback).Run(has_authenticator_);
  }

 private:
  const bool has_authenticator_;
};

class ChromePaymentRequestTestDelegate : public ChromePaymentRequestDelegate {
 public:
  ChromePaymentRequestTestDelegate(content::RenderFrameHost* render_frame_host,
                                   bool is_off_the_record,
                                   bool valid_ssl,
                                   PrefService* prefs,
                                   const std::string& twa_package_name,
                                   bool has_authenticator,
                                   PaymentUIObserver* ui_observer_for_test)
      : ChromePaymentRequestDelegate(render_frame_host),
        frame_routing_id_(content::GlobalFrameRoutingId(
            render_frame_host->GetProcess()->GetID(),
            render_frame_host->GetRoutingID())),
        is_off_the_record_(is_off_the_record),
        valid_ssl_(valid_ssl),
        prefs_(prefs),
        twa_package_name_(twa_package_name),
        has_authenticator_(has_authenticator),
        ui_observer_for_test_(ui_observer_for_test) {}

  bool IsOffTheRecord() const override { return is_off_the_record_; }
  std::string GetInvalidSslCertificateErrorMessage() override {
    return valid_ssl_ ? "" : "Invalid SSL certificate";
  }
  PrefService* GetPrefService() override { return prefs_; }
  bool IsBrowserWindowActive() const override { return true; }
  std::string GetTwaPackageName() const override { return twa_package_name_; }
  std::unique_ptr<autofill::InternalAuthenticator> CreateInternalAuthenticator()
      const override {
    auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
    return rfh ? std::make_unique<TestAuthenticator>(rfh->GetMainFrame(),
                                                     has_authenticator_)
               : nullptr;
  }
  const PaymentUIObserver* GetPaymentUIObserver() const override {
    return ui_observer_for_test_;
  }

 private:
  content::GlobalFrameRoutingId frame_routing_id_;
  const bool is_off_the_record_;
  const bool valid_ssl_;
  PrefService* const prefs_;
  const std::string twa_package_name_;
  const bool has_authenticator_;
  const PaymentUIObserver* const ui_observer_for_test_;
};

}  // namespace

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

    controller_->OnAppListReady();
  }
  void OnNotSupportedError() override { controller_->OnNotSupportedError(); }
  void OnConnectionTerminated() override {
    controller_->OnConnectionTerminated();
  }
  void OnAbortCalled() override { controller_->OnAbortCalled(); }
  void OnCompleteCalled() override { controller_->OnCompleteCalled(); }

  // PaymentUIObserver:
  void OnUIDisplayed() const override { controller_->OnUIDisplayed(); }

 private:
  PaymentRequestTestController* const controller_;
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
  if (!delegate_)
    return false;

  PaymentRequestDialog* dialog = delegate_->GetDialogForTesting();
  if (!dialog)
    return false;

  dialog->ConfirmPaymentForTesting();
  return true;
}

bool PaymentRequestTestController::ClickPaymentHandlerCloseButton() {
  if (!delegate_)
    return false;

  PaymentRequestDialog* dialog = delegate_->GetDialogForTesting();
  if (!dialog)
    return false;

  dialog->CloseDialog();
  return true;
}

bool PaymentRequestTestController::ConfirmMinimalUI() {
  // Desktop does not have a minimal UI.
  return true;
}

bool PaymentRequestTestController::DismissMinimalUI() {
  // Desktop does not have a minimal UI.
  return true;
}

bool PaymentRequestTestController::IsAndroidMarshmallowOrLollipop() {
  return false;
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
      [](ObserverConverter* observer_for_test, bool is_off_the_record,
         bool valid_ssl, PrefService* prefs,
         const std::string& twa_package_name, bool has_authenticator,
         const std::string& twa_payment_app_method_name,
         const std::string& twa_payment_app_response,
         base::WeakPtr<ContentPaymentRequestDelegate>* delegate_weakptr,
         mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
         content::RenderFrameHost* render_frame_host) {
        DCHECK(render_frame_host);
        DCHECK(render_frame_host->IsCurrent());
        auto delegate = std::make_unique<ChromePaymentRequestTestDelegate>(
            render_frame_host, is_off_the_record, valid_ssl, prefs,
            twa_package_name, has_authenticator, observer_for_test);
        *delegate_weakptr = delegate->GetContentWeakPtr();
        PaymentRequestWebContentsManager* manager =
            PaymentRequestWebContentsManager::GetOrCreateForWebContents(
                content::WebContents::FromRenderFrameHost(render_frame_host));
        if (!twa_payment_app_method_name.empty()) {
          AndroidAppCommunication::GetForBrowserContext(
              render_frame_host->GetBrowserContext())
              ->SetAppForTesting(twa_package_name, twa_payment_app_method_name,
                                 twa_payment_app_response);
        }
        manager->CreatePaymentRequest(render_frame_host, std::move(delegate),
                                      std::move(receiver), observer_for_test);
      },
      observer_converter_.get(), is_off_the_record_, valid_ssl_, prefs_.get(),
      twa_package_name_, has_authenticator_, twa_payment_app_method_name_,
      twa_payment_app_response_, &delegate_));
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

void PaymentRequestTestController::OnCompleteCalled() {
  if (observer_) {
    observer_->OnCompleteCalled();
  }
}

void PaymentRequestTestController::OnMinimalUIReady() {
  NOTREACHED();
}

void PaymentRequestTestController::OnUIDisplayed() {
  if (observer_) {
    observer_->OnUIDisplayed();
  }
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
