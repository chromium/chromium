// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_android.h"

#include <memory>

namespace autofill::payments {

// static
std::unique_ptr<ChromePaymentsAutofillClient>
ChromePaymentsAutofillClient::Create(ContentAutofillClient* client) {
  return std::make_unique<ChromePaymentsAutofillClientAndroid>(client);
}

ChromePaymentsAutofillClientAndroid::ChromePaymentsAutofillClientAndroid(
    ContentAutofillClient* client)
    : ChromePaymentsAutofillClient(client) {}

ChromePaymentsAutofillClientAndroid::~ChromePaymentsAutofillClientAndroid() =
    default;

}  // namespace autofill::payments
