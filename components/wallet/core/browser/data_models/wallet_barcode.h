// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_BARCODE_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_BARCODE_H_

#include <string>

namespace wallet {

// Represents the format of a barcode. This enum corresponds to the
// BarcodeFormat enum in the Shape Detection API.
// https://wicg.github.io/shape-detection-api/#barcodeformat-section
enum class WalletBarcodeFormat {
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

struct WalletBarcode {
  std::string raw_value;
  WalletBarcodeFormat format;

  bool operator==(const WalletBarcode& other) const = default;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_WALLET_BARCODE_H_
