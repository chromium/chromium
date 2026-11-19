// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_BARCODE_DETECTOR_IMPL_H_

#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"
#include "components/wallet/content/common/mojom/image_extractor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace wallet {

class WalletablePassBarcodeDetectorImpl : public WalletablePassBarcodeDetector {
 public:
  WalletablePassBarcodeDetectorImpl();
  ~WalletablePassBarcodeDetectorImpl() override;

  // WalletablePassBarcodeDetector:
  void Detect(
      content::WebContents* web_contents,
      mojom::ImageExtractionOptionsPtr extraction_options,
      std::vector<shape_detection::mojom::BarcodeFormat> barcode_formats,
      WalletBarcodeDetectionDetectCallback callback) override;

 private:
  // The barcodes decoded from one image, and the element it came from.
  using ImageDetectionResults =
      std::pair<WalletBarcodeSourceElement,
                std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>>;

  // Creates and binds a remote for the image extractor service for the given
  // `web_contents`.
  virtual mojo::Remote<mojom::ImageExtractor> CreateAndBindImageExtractorRemote(
      content::WebContents* web_contents);

  // Creates and binds a remote for the barcode detection service with
  // `options`.
  virtual mojo::Remote<shape_detection::mojom::BarcodeDetection>
  CreateAndBindBarcodeDetectorRemote(
      shape_detection::mojom::BarcodeDetectorOptionsPtr options);

  // Takes a remote `ImageExtractor` and extracts the images that pass
  // `extraction_options` from the web contents, then decodes
  // `barcode_formats` in them. `callback` is only triggered once all the
  // images are extracted.
  void ExtractImageWithRemote(
      mojo::Remote<mojom::ImageExtractor> remote,
      mojom::ImageExtractionOptionsPtr extraction_options,
      std::vector<shape_detection::mojom::BarcodeFormat> barcode_formats,
      WalletBarcodeDetectionDetectCallback callback);

  // Invoked when images have been extracted from the web contents. This method
  // will detect `barcode_formats` in the extracted images.
  void OnImagesExtracted(
      std::vector<shape_detection::mojom::BarcodeFormat> barcode_formats,
      WalletBarcodeDetectionDetectCallback callback,
      std::vector<mojom::ExtractedImagePtr> images);

  // Invoked when all barcode detections are complete. Each element of
  // `all_detected_barcodes` holds the barcodes decoded from one image, together
  // with the element that the image came from.
  void OnAllBarcodesDetected(
      WalletBarcodeDetectionDetectCallback callback,
      std::vector<ImageDetectionResults> all_detected_barcodes);

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
