// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"

#include "base/functional/callback.h"

namespace wallet {

WalletablePassBarcodeDetectorImpl::WalletablePassBarcodeDetectorImpl() =
    default;

WalletablePassBarcodeDetectorImpl::~WalletablePassBarcodeDetectorImpl() =
    default;

WalletBarcodeFormat WalletablePassBarcodeDetectorImpl::MojoToBarcodeFormat(
    shape_detection::mojom::BarcodeFormat format) {
  // TODO(crbug.com/438364540): Implement the conversion logic from Mojo format
  // to WalletBarcodeFormat.
  return WalletBarcodeFormat::UNKNOWN;
}

void WalletablePassBarcodeDetectorImpl::Detect(
    content::WebContents* web_contents,
    WalletBarcodeDetectionDetectCallback callback) {
  // TODO(crbug.com/438364540): Implement the actual detection logic here.
  std::move(callback).Run({});
}

}  // namespace wallet
