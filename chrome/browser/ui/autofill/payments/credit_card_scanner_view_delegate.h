// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"

namespace autofill {

class CreditCard;

// Receives notifications when credit card scanner UI is dismissed either due to
// user cancelling the scan or successfully completing the scan.
class CreditCardScannerViewDelegate {
 public:
  // Called when user cancelled the scan.
  virtual void ScanCancelled() = 0;

  // Called when the scan completed successfully.
  virtual void ScanCompleted(const CreditCard& card) = 0;

 protected:
  // Destroys the delegate.
  virtual ~CreditCardScannerViewDelegate() {}
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_DELEGATE_H_
