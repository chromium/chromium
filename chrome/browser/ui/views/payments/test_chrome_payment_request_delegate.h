// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

class PrefService;

namespace autofill {
class RegionDataLoader;
}  // namespace autofill

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

// Implementation of the Payment Request delegate used in tests.
class TestChromePaymentRequestDelegate : public ChromePaymentRequestDelegate {
 public:
  // This delegate does not own things passed as pointers.
  explicit TestChromePaymentRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~TestChromePaymentRequestDelegate() override;

  TestChromePaymentRequestDelegate(const TestChromePaymentRequestDelegate&) =
      delete;
  TestChromePaymentRequestDelegate& operator=(
      const TestChromePaymentRequestDelegate&) = delete;

  // If an Override* method is not called, then the default implementation from
  // ChromePaymentRequestDelegate is used.
  void OverrideRegionDataLoader(autofill::RegionDataLoader* region_data_loader);
  void OverridePrefService(PrefService* pref_service);
  void OverrideOffTheRecord(bool is_off_the_record);
  void OverrideValidSSL(bool is_valid_ssl);
  void OverrideBrowserWindowActive(bool is_browser_window_active);

  void set_payment_ui_observer(
      base::WeakPtr<PaymentUIObserver> payment_ui_observer) {
    payment_ui_observer_ = payment_ui_observer;
  }

  void set_payment_request_dialog_view_observer_for_test(
      base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer) {
    observer_ = observer;
  }

  void set_twa_package_name(const std::string& twa_package_name) {
    twa_package_name_ = twa_package_name;
  }

  void set_has_authenticator(bool has_authenticator) {
    has_authenticator_ = has_authenticator;
  }

  PaymentRequestDialogView* dialog_view() {
    return static_cast<PaymentRequestDialogView*>(shown_dialog_.get());
  }

 private:
  // ChromePaymentRequestDelegate:
  void ShowDialog(base::WeakPtr<PaymentRequest> request) override;
  bool IsOffTheRecord() const override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;
  std::unique_ptr<webauthn::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  std::string GetInvalidSslCertificateErrorMessage() override;
  void GetTwaPackageName(GetTwaPackageNameCallback callback) const override;
  const base::WeakPtr<PaymentUIObserver> GetPaymentUIObserver() const override;

  // Not owned so must outlive the PaymentRequest object;
  raw_ptr<autofill::RegionDataLoader> region_data_loader_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;

  base::WeakPtr<PaymentUIObserver> payment_ui_observer_;
  base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer_;

  std::string twa_package_name_;

  bool has_authenticator_ = true;

  std::optional<bool> is_off_the_record_;
  std::optional<bool> is_valid_ssl_;
  std::optional<bool> is_browser_window_active_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_
