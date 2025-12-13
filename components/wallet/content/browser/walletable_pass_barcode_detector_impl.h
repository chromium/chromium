// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"
#include "components/wallet/content/common/mojom/image_extractor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace wallet {

class WalletablePassBarcodeDetectorImpl : public WalletablePassBarcodeDetector {
 public:
  WalletablePassBarcodeDetectorImpl();
  ~WalletablePassBarcodeDetectorImpl() override;

  // WalletablePassBarcodeDetector:
  void Detect(content::WebContents* web_contents,
              WalletBarcodeDetectionDetectCallback callback) override;

 private:
  // Creates and binds a remote for the image extractor service for the given
  // `web_contents`.
  virtual mojo::Remote<mojom::ImageExtractor> CreateAndBindImageExtractorRemote(
      content::WebContents* web_contents);

  // Creates and binds a remote for the barcode detection service.
  virtual mojo::Remote<shape_detection::mojom::BarcodeDetection>
  CreateAndBindBarcodeDetectorRemote();

  // Takes a remote `ImageExtractor` and extracts images from the web contents.
  // `callback` is only triggered once all the images are extracted.
  void ExtractImageWithRemote(mojo::Remote<mojom::ImageExtractor> remote,
                              WalletBarcodeDetectionDetectCallback callback);

  // Invoked when images have been extracted from the web contents. This method
  // will detect barcodes in the extracted images.
  void OnImagesExtracted(WalletBarcodeDetectionDetectCallback callback,
                         const std::vector<SkBitmap>& images);

  // Invoked when all barcode detections are complete.
  void OnAllBarcodesDetected(
      WalletBarcodeDetectionDetectCallback callback,
      std::vector<
          std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>>
          all_results);

  // Invoked when the image extractor remote is disconnected.
  void OnRemoteDisconnected(WalletBarcodeDetectionDetectCallback callback);

  // Invoked when the barcode detector remote is disconnected.
  void OnBarcodeDetectorDisconnected(
      WalletBarcodeDetectionDetectCallback callback);

  // Converts a shape_detection::mojom::BarcodeFormat to a
  // wallet::WalletBarcodeFormat.
  WalletBarcodeFormat MojoToBarcodeFormat(
      shape_detection::mojom::BarcodeFormat format);

  mojo::Remote<shape_detection::mojom::BarcodeDetection> barcode_detector_;
  mojo::Remote<mojom::ImageExtractor> image_extractor_remote_;

  base::WeakPtrFactory<WalletablePassBarcodeDetectorImpl> weak_ptr_factory_{
      this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_
