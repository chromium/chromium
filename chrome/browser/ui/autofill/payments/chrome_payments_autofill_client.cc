// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill::payments {

ChromePaymentsAutofillClient::ChromePaymentsAutofillClient(
    content::WebContents* web_contents) {
  content::WebContentsObserver::Observe(web_contents);
}

ChromePaymentsAutofillClient::~ChromePaymentsAutofillClient() = default;

}  // namespace autofill::payments
