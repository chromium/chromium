// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

// Represents the format of a barcode. This enum corresponds to the
// BarcodeFormat enum in the Shape Detection API.
// https://wicg.github.io/shape-detection-api/#barcodeformat-section
enum WalletBarcodeFormat {
  AZTEC,
  CODE_128,
  CODE_39,
  CODE_93,
  CODABAR,
  DATA_MATRIX,
  EAN_13,
  EAN_8,
  ITF,
  PDF417,
  QR_CODE,
  UNKNOWN,
  UPC_A,
  UPC_E
};

struct WalletBarcodeDetectionResult {
  std::string raw_value;
  WalletBarcodeFormat format;

  bool operator==(const WalletBarcodeDetectionResult& other) const = default;
};

// A class used to detect barcodes from a potential walletable pass web
// contents.
class WalletablePassBarcodeDetector {
 public:
  using WalletBarcodeDetectionDetectCallback = base::OnceCallback<void(
      const std::vector<WalletBarcodeDetectionResult>&)>;

  virtual ~WalletablePassBarcodeDetector() = default;

  // Detects barcodes in images on current `web_contents`.
  virtual void Detect(content::WebContents* web_contents,
                      WalletBarcodeDetectionDetectCallback callback) = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_H_
