// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"

class GURL;

namespace autofill {

class ChromeAutofillClient;

namespace payments {

// Desktop implementation of the PaymentsWindowManager interface. One per
// WebContents, owned by the ChromeAutofillClient associated with the
// WebContents of the original tab that the pop-up is created in.
class DesktopPaymentsWindowManager : public PaymentsWindowManager {
 public:
  explicit DesktopPaymentsWindowManager(ChromeAutofillClient* client);
  DesktopPaymentsWindowManager(const DesktopPaymentsWindowManager&) = delete;
  DesktopPaymentsWindowManager& operator=(const DesktopPaymentsWindowManager&) =
      delete;
  ~DesktopPaymentsWindowManager() override;

  void CreatePopupForTesting(const GURL& url) { CreatePopup(url); }

 private:
  void CreatePopup(const GURL& url);

  // ChromeAutofillClient that owns `this`.
  const raw_ref<ChromeAutofillClient> client_;
};

}  // namespace payments

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_H_
