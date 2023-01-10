// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/payments/core/error_logger.h"
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace payments {
namespace {

class TestAuthenticator : public content::InternalAuthenticatorImpl {
 public:
  TestAuthenticator(content::RenderFrameHost* rfh, bool has_authenticator)
      : content::InternalAuthenticatorImpl(rfh),
        has_authenticator_(has_authenticator) {}

  ~TestAuthenticator() override = default;

  // webauthn::InternalAuthenticator
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override {
    std::move(callback).Run(has_authenticator_);
  }

 private:
  const bool has_authenticator_;
};

}  // namespace

TestChromePaymentRequestDelegate::TestChromePaymentRequestDelegate(
    content::RenderFrameHost* render_frame_host)
    : ChromePaymentRequestDelegate(render_frame_host) {}

TestChromePaymentRequestDelegate::~TestChromePaymentRequestDelegate() = default;

void TestChromePaymentRequestDelegate::OverrideRegionDataLoader(
    autofill::RegionDataLoader* region_data_loader) {
  region_data_loader_ = region_data_loader;
}

void TestChromePaymentRequestDelegate::OverridePrefService(
    PrefService* pref_service) {
  pref_service_ = pref_service;
}

void TestChromePaymentRequestDelegate::OverrideOffTheRecord(
    bool is_off_the_record) {
  is_off_the_record_ = is_off_the_record;
}

void TestChromePaymentRequestDelegate::OverrideValidSSL(bool is_valid_ssl) {
  is_valid_ssl_ = is_valid_ssl;
}

void TestChromePaymentRequestDelegate::OverrideBrowserWindowActive(
    bool is_browser_window_active) {
  is_browser_window_active_ = is_browser_window_active;
}

void TestChromePaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  if (dialog_type_ == DialogType::PAYMENT_REQUEST) {
    shown_dialog_ = PaymentRequestDialogView::Create(request, observer_);
    shown_dialog_->ShowDialog();
  } else {
    ChromePaymentRequestDelegate::ShowDialog(request);
  }
}

bool TestChromePaymentRequestDelegate::IsOffTheRecord() const {
  return is_off_the_record_.has_value()
             ? *is_off_the_record_
             : ChromePaymentRequestDelegate::IsOffTheRecord();
}

autofill::RegionDataLoader*
TestChromePaymentRequestDelegate::GetRegionDataLoader() {
  return region_data_loader_
             ? region_data_loader_.get()
             : ChromePaymentRequestDelegate::GetRegionDataLoader();
}

PrefService* TestChromePaymentRequestDelegate::GetPrefService() {
  return pref_service_ ? pref_service_.get()
                       : ChromePaymentRequestDelegate::GetPrefService();
}

bool TestChromePaymentRequestDelegate::IsBrowserWindowActive() const {
  return is_browser_window_active_.has_value()
             ? *is_browser_window_active_
             : ChromePaymentRequestDelegate::IsBrowserWindowActive();
}

std::unique_ptr<webauthn::InternalAuthenticator>
TestChromePaymentRequestDelegate::CreateInternalAuthenticator() const {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  return rfh ? std::make_unique<TestAuthenticator>(rfh, has_authenticator_)
             : nullptr;
}

std::string
TestChromePaymentRequestDelegate::GetInvalidSslCertificateErrorMessage() {
  if (is_valid_ssl_.has_value())
    return *is_valid_ssl_ ? "" : "Invalid SSL certificate";

  return ChromePaymentRequestDelegate::GetInvalidSslCertificateErrorMessage();
}

void TestChromePaymentRequestDelegate::GetTwaPackageName(
    GetTwaPackageNameCallback callback) const {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), twa_package_name_));
}

const base::WeakPtr<PaymentUIObserver>
TestChromePaymentRequestDelegate::GetPaymentUIObserver() const {
  return payment_ui_observer_;
}

}  // namespace payments
