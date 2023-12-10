// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

// Chrome implementation of PaymentsAutofillClient. Used for Chrome Desktop and
// Clank. Owned by the ChromeAutofillClient. Created lazily in the
// ChromeAutofillClient when it is needed.
class ChromePaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  ChromePaymentsAutofillClient();
  ChromePaymentsAutofillClient(const ChromePaymentsAutofillClient&) = delete;
  ChromePaymentsAutofillClient& operator=(const ChromePaymentsAutofillClient&) =
      delete;
  ~ChromePaymentsAutofillClient() override;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_AUTOFILL_CLIENT_H_
