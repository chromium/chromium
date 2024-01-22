// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_WINDOW_MANAGER_H_

#include "components/autofill/core/browser/payments/payments_window_manager.h"

namespace autofill::payments {

// Chrome implementation of the PaymentsWindowManager interface. To be used
// for Desktop and Clank payments autofill pop-up flows. One per WebContents,
// owned by the ChromeAutofillClient associated with the WebContents of the
// original tab that the pop-up is created in.
class ChromePaymentsWindowManager : public PaymentsWindowManager {
 public:
  ChromePaymentsWindowManager();
  ChromePaymentsWindowManager(const ChromePaymentsWindowManager&) = delete;
  ChromePaymentsWindowManager& operator=(const ChromePaymentsWindowManager&) =
      delete;
  ~ChromePaymentsWindowManager() override;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CHROME_PAYMENTS_WINDOW_MANAGER_H_
