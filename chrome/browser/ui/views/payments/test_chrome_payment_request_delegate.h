// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

class PrefService;

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequest;

// Implementation of the Payment Request delegate used in tests.
class TestChromePaymentRequestDelegate : public ChromePaymentRequestDelegate {
 public:
  // This delegate does not own things passed as pointers.
  TestChromePaymentRequestDelegate(
      content::WebContents* web_contents,
      PaymentRequestDialogView::ObserverForTest* observer,
      PrefService* pref_service,
      bool is_incognito,
      bool is_valid_ssl,
      bool is_browser_window_active,
      bool skip_ui_for_basic_card);

  void SetRegionDataLoader(autofill::RegionDataLoader* region_data_loader) {
    region_data_loader_ = region_data_loader;
  }

  // ChromePaymentRequestDelegate.
  void ShowDialog(PaymentRequest* request) override;
  bool IsIncognito() const override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;
  std::string GetInvalidSslCertificateErrorMessage() override;
  bool SkipUiForBasicCard() const override;

  PaymentRequestDialogView* dialog_view() {
    return static_cast<PaymentRequestDialogView*>(shown_dialog_);
  }

 private:
  // Not owned so must outlive the PaymentRequest object;
  autofill::RegionDataLoader* region_data_loader_;

  PaymentRequestDialogView::ObserverForTest* observer_;
  PrefService* pref_service_;
  const bool is_incognito_;
  const bool is_valid_ssl_;
  const bool is_browser_window_active_;
  const bool skip_ui_for_basic_card_;

  DISALLOW_COPY_AND_ASSIGN(TestChromePaymentRequestDelegate);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_TEST_CHROME_PAYMENT_REQUEST_DELEGATE_H_
