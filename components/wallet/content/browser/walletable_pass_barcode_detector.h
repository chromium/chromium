// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

// A class used to detect barcodes from a potential walletable pass web
// contents.
class WalletablePassBarcodeDetector {
 public:
  using WalletBarcodeDetectionDetectCallback =
      base::OnceCallback<void(const std::vector<WalletBarcode>&)>;

  virtual ~WalletablePassBarcodeDetector() = default;

  // Detects barcodes in images on current `web_contents`.
  virtual void Detect(content::WebContents* web_contents,
                      WalletBarcodeDetectionDetectCallback callback) = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_
