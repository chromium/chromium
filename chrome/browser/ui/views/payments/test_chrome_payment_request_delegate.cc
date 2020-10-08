// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_chrome_payment_request_delegate.h"

#include "content/public/browser/web_contents.h"

namespace payments {

TestChromePaymentRequestDelegate::TestChromePaymentRequestDelegate(
    content::RenderFrameHost* render_frame_host,
    PaymentRequestDialogView::ObserverForTest* observer,
    PrefService* pref_service,
    bool is_off_the_record,
    bool is_valid_ssl,
    bool is_browser_window_active,
    bool skip_ui_for_basic_card)
    : ChromePaymentRequestDelegate(render_frame_host),
      region_data_loader_(nullptr),
      observer_(observer),
      pref_service_(pref_service),
      is_off_the_record_(is_off_the_record),
      is_valid_ssl_(is_valid_ssl),
      is_browser_window_active_(is_browser_window_active),
      skip_ui_for_basic_card_(skip_ui_for_basic_card) {}

void TestChromePaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  shown_dialog_ = PaymentRequestDialogView::Create(request, observer_);
  shown_dialog_->ShowDialog();
}

bool TestChromePaymentRequestDelegate::IsOffTheRecord() const {
  return is_off_the_record_;
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
