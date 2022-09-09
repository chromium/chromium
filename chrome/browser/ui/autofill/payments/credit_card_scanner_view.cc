// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view.h"

#include "build/build_config.h"

namespace autofill {

// Not implemented on other platforms yet.
#if !BUILDFLAG(IS_ANDROID)
// static
bool CreditCardScannerView::CanShow() {
  return false;
}

// static
std::unique_ptr<CreditCardScannerView> CreditCardScannerView::Create(
    const base::WeakPtr<CreditCardScannerViewDelegate>& delegate,
    content::WebContents* web_contents) {
  return nullptr;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill
