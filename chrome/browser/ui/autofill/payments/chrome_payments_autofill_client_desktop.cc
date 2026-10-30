// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_desktop.h"

#include <memory>

namespace autofill::payments {

// static
std::unique_ptr<ChromePaymentsAutofillClient>
ChromePaymentsAutofillClient::Create(ContentAutofillClient* client) {
  return std::make_unique<ChromePaymentsAutofillClientDesktop>(client);
}

ChromePaymentsAutofillClientDesktop::ChromePaymentsAutofillClientDesktop(
    ContentAutofillClient* client)
    : ChromePaymentsAutofillClient(client) {}

ChromePaymentsAutofillClientDesktop::~ChromePaymentsAutofillClientDesktop() =
    default;

}  // namespace autofill::payments
