// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_DESKTOP_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_DESKTOP_H_

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

namespace autofill {

class ContentAutofillClient;

namespace payments {

// Desktop implementation of PaymentsAutofillClient for Chrome. Owned by
// ChromeAutofillClient.
class ChromePaymentsAutofillClientDesktop
    : public ChromePaymentsAutofillClient {
 public:
  explicit ChromePaymentsAutofillClientDesktop(ContentAutofillClient* client);
  ChromePaymentsAutofillClientDesktop(
      const ChromePaymentsAutofillClientDesktop&) = delete;
  ChromePaymentsAutofillClientDesktop& operator=(
      const ChromePaymentsAutofillClientDesktop&) = delete;
  ~ChromePaymentsAutofillClientDesktop() override;
};

}  // namespace payments
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_DESKTOP_H_
