// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_CONTROLLER_H_

#include "components/autofill/core/browser/autofill_client.h"

namespace content {
class WebContents;
}

namespace autofill {

// Controller for the credit card scanner UI. The controller deletes itself
// after the view is dismissed.
class CreditCardScannerController {
 public:
  // Returns true if both platform and device support scanning credit cards. The
  // platform must have the required APIs. The device must have, e.g., a camera.
  static bool HasCreditCardScanFeature();

  // Shows the UI to scan a credit card. The UI is associated with the
  // |web_contents|. Notifies the |delegate| when scanning completes
  // successfully. Destroys itself when the UI is dismissed. Should be called
  // only if HasCreditCardScanScanFeature() returns true.
  static void ScanCreditCard(
      content::WebContents* web_contents,
      const AutofillClient::CreditCardScanCallback& callback);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_CONTROLLER_H_
