// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_

namespace autofill::payments {

// A payments-specific client interface that handles dependency injection, and
// its implementations serve as the integration for platform-specific code. One
// per WebContents, owned by the AutofillClient. Created lazily in the
// AutofillClient when it is needed.
class PaymentsAutofillClient {
 public:
  virtual ~PaymentsAutofillClient();
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_AUTOFILL_CLIENT_H_
