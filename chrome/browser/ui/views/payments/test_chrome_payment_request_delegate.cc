// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"

#include "content/public/browser/web_contents.h"

namespace payments {

TestChromePaymentRequestDelegate::TestChromePaymentRequestDelegate(
    content::WebContents* web_contents,
    PaymentRequestDialogView::ObserverForTest* observer,
    PrefService* pref_service,
    bool is_incognito,
    bool is_valid_ssl,
    bool is_browser_window_active,
    bool skip_ui_for_basic_card)
    : ChromePaymentRequestDelegate(web_contents),
      region_data_loader_(nullptr),
      observer_(observer),
      pref_service_(pref_service),
      is_incognito_(is_incognito),
      is_valid_ssl_(is_valid_ssl),
      is_browser_window_active_(is_browser_window_active),
      skip_ui_for_basic_card_(skip_ui_for_basic_card) {}

void TestChromePaymentRequestDelegate::ShowDialog(PaymentRequest* request) {
  shown_dialog_ = new PaymentRequestDialogView(request, observer_);
  shown_dialog_->ShowDialog();
}

bool TestChromePaymentRequestDelegate::IsIncognito() const {
  return is_incognito_;
}

autofill::RegionDataLoader*
TestChromePaymentRequestDelegate::GetRegionDataLoader() {
  if (region_data_loader_)
    return region_data_loader_;
  return ChromePaymentRequestDelegate::GetRegionDataLoader();
}

PrefService* TestChromePaymentRequestDelegate::GetPrefService() {
  return pref_service_;
}

bool TestChromePaymentRequestDelegate::IsBrowserWindowActive() const {
  return is_browser_window_active_;
}

std::string
TestChromePaymentRequestDelegate::GetInvalidSslCertificateErrorMessage() {
  return is_valid_ssl_ ? "" : "Invalid SSL certificate";
}

bool TestChromePaymentRequestDelegate::SkipUiForBasicCard() const {
  return skip_ui_for_basic_card_;
}

}  // namespace payments
