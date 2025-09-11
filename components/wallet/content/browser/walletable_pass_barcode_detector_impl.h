// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"

namespace wallet {

class WalletablePassBarcodeDetectorImpl : public WalletablePassBarcodeDetector {
 public:
  WalletablePassBarcodeDetectorImpl();
  ~WalletablePassBarcodeDetectorImpl() override;

  // WalletablePassBarcodeDetector:
  void Detect(content::WebContents* web_contents,
              WalletBarcodeDetectionDetectCallback callback) override;

 private:
  WalletBarcodeFormat MojoToBarcodeFormat(
      shape_detection::mojom::BarcodeFormat format);
  base::WeakPtrFactory<WalletablePassBarcodeDetectorImpl> weak_ptr_factory_{
      this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_
