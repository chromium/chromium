// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace autofill {

class CreditCardScannerViewDelegate;

// View for the credit card scanner UI. Owned by the controller.
class CreditCardScannerView {
 public:
  // Returns true if the platform implements the credit card scanner UI and the
  // device supports scanning credit cards, e.g., it has a camera.
  static bool CanShow();

  // Creates a view for the credit card scanner UI. The view is associated with
  // the |web_contents| and notifies the |delegate| when a scan is cancelled or
  // completed. Should be called only if CanShow() returns true.
  static std::unique_ptr<CreditCardScannerView> Create(
      const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
      content::WebContents* web_contents);

  virtual ~CreditCardScannerView() {}

  // Shows the UI for scanning credit cards.
  virtual void Show() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CREDIT_CARD_SCANNER_VIEW_H_
