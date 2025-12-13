// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shape_detection_service.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"

namespace wallet {

WalletablePassBarcodeDetectorImpl::WalletablePassBarcodeDetectorImpl() =
    default;

WalletablePassBarcodeDetectorImpl::~WalletablePassBarcodeDetectorImpl() =
    default;

WalletBarcodeFormat WalletablePassBarcodeDetectorImpl::MojoToBarcodeFormat(
    shape_detection::mojom::BarcodeFormat format) {
  switch (format) {
    case shape_detection::mojom::BarcodeFormat::AZTEC:
      return WalletBarcodeFormat::AZTEC;
    case shape_detection::mojom::BarcodeFormat::CODE_128:
      return WalletBarcodeFormat::CODE_128;
    case shape_detection::mojom::BarcodeFormat::CODE_39:
      return WalletBarcodeFormat::CODE_39;
    case shape_detection::mojom::BarcodeFormat::CODE_93:
      return WalletBarcodeFormat::CODE_93;
    case shape_detection::mojom::BarcodeFormat::CODABAR:
      return WalletBarcodeFormat::CODABAR;
    case shape_detection::mojom::BarcodeFormat::DATA_MATRIX:
      return WalletBarcodeFormat::DATA_MATRIX;
    case shape_detection::mojom::BarcodeFormat::EAN_13:
      return WalletBarcodeFormat::EAN_13;
    case shape_detection::mojom::BarcodeFormat::EAN_8:
      return WalletBarcodeFormat::EAN_8;
    case shape_detection::mojom::BarcodeFormat::ITF:
      return WalletBarcodeFormat::ITF;
    case shape_detection::mojom::BarcodeFormat::PDF417:
      return WalletBarcodeFormat::PDF417;
    case shape_detection::mojom::BarcodeFormat::QR_CODE:
      return WalletBarcodeFormat::QR_CODE;
    case shape_detection::mojom::BarcodeFormat::UPC_A:
      return WalletBarcodeFormat::UPC_A;
    case shape_detection::mojom::BarcodeFormat::UPC_E:
      return WalletBarcodeFormat::UPC_E;
    case shape_detection::mojom::BarcodeFormat::UNKNOWN:
      return WalletBarcodeFormat::UNKNOWN;
  }

  NOTREACHED();
}

mojo::Remote<mojom::ImageExtractor>
WalletablePassBarcodeDetectorImpl::CreateAndBindImageExtractorRemote(
    content::WebContents* web_contents) {
  DCHECK(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  mojo::Remote<mojom::ImageExtractor> remote;
  web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      remote.BindNewPipeAndPassReceiver());
  return remote;
}

mojo::Remote<shape_detection::mojom::BarcodeDetection>
WalletablePassBarcodeDetectorImpl::CreateAndBindBarcodeDetectorRemote() {
  mojo::Remote<shape_detection::mojom::BarcodeDetection> remote;
  mojo::Remote<shape_detection::mojom::BarcodeDetectionProvider> provider;
  content::GetShapeDetectionService()->BindBarcodeDetectionProvider(
      provider.BindNewPipeAndPassReceiver());
  auto options = shape_detection::mojom::BarcodeDetectorOptions::New();
  provider->CreateBarcodeDetection(remote.BindNewPipeAndPassReceiver(),
                                   std::move(options));
  return remote;
}

void WalletablePassBarcodeDetectorImpl::ExtractImageWithRemote(
    mojo::Remote<mojom::ImageExtractor> remote,
    WalletBarcodeDetectionDetectCallback callback) {
  if (!remote) {
    std::move(callback).Run({});
    return;
  }

  // If a previous image extraction is in progress, reset it to cancel.
  if (image_extractor_remote_) {
    // TODO(crbug.com/453602282): Record a metric for the cancellation.
    image_extractor_remote_.reset();
  }
  image_extractor_remote_ = std::move(remote);

  auto [success_cb, disconnected_cb] =
      base::SplitOnceCallback(std::move(callback));

  image_extractor_remote_.set_disconnect_handler(base::BindOnce(
      &WalletablePassBarcodeDetectorImpl::OnRemoteDisconnected,
      weak_ptr_factory_.GetWeakPtr(), std::move(disconnected_cb)));

  image_extractor_remote_->ExtractImages(
      base::BindOnce(&WalletablePassBarcodeDetectorImpl::OnImagesExtracted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(success_cb)));
}

void WalletablePassBarcodeDetectorImpl::OnRemoteDisconnected(
    WalletBarcodeDetectionDetectCallback callback) {
  image_extractor_remote_.reset();
  std::move(callback).Run({});
}

void WalletablePassBarcodeDetectorImpl::OnImagesExtracted(
    WalletBarcodeDetectionDetectCallback callback,
    const std::vector<SkBitmap>& images) {
  image_extractor_remote_.reset();

  // Don't start a new detection if there are no images.
  if (images.empty()) {
    std::move(callback).Run({});
    return;
  }

  // If a previous detection is in progress, reset it to cancel.
  if (barcode_detector_) {
    // TODO(crbug.com/453602282): Record a metric for the cancellation.
    barcode_detector_.reset();
  }

  // Split the callback into two parts. The `success_callback` will be called
  // when all barcode detections complete successfully. The `failure_callback`
  // will be called if the mojo connection to the barcode detection service
  // is lost, which is handled by the disconnect handler. Only one of these
  // callbacks will ever be invoked.
  auto [success_callback, failure_callback] =
      base::SplitOnceCallback(std::move(callback));

  barcode_detector_ = CreateAndBindBarcodeDetectorRemote();
  barcode_detector_.set_disconnect_handler(base::BindOnce(
      &WalletablePassBarcodeDetectorImpl::OnBarcodeDetectorDisconnected,
      weak_ptr_factory_.GetWeakPtr(), std::move(failure_callback)));

  // The barrier callback will be called after all images have been processed.
  auto barrier_callback = base::BarrierCallback<
      std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>>(
      images.size(),
      base::BindOnce(&WalletablePassBarcodeDetectorImpl::OnAllBarcodesDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)));

  for (const SkBitmap& image : images) {
    barcode_detector_->Detect(image, barrier_callback);
  }
}

void WalletablePassBarcodeDetectorImpl::OnAllBarcodesDetected(
    WalletBarcodeDetectionDetectCallback callback,
    std::vector<std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>>
        all_detected_barcodes) {
  std::vector<WalletBarcode> results;
  for (const std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>&
           detected_barcodes : all_detected_barcodes) {
    for (const shape_detection::mojom::BarcodeDetectionResultPtr& barcode :
         detected_barcodes) {
      if (!barcode) {
        continue;  // Skip null result.
      }
      results.push_back({.raw_value = barcode->raw_value,
                         .format = MojoToBarcodeFormat(barcode->format)});
    }
  }
  barcode_detector_.reset();
  std::move(callback).Run(results);
}

void WalletablePassBarcodeDetectorImpl::OnBarcodeDetectorDisconnected(
    WalletBarcodeDetectionDetectCallback callback) {
  barcode_detector_.reset();
  std::move(callback).Run({});
}

void WalletablePassBarcodeDetectorImpl::Detect(
    content::WebContents* web_contents,
    WalletBarcodeDetectionDetectCallback callback) {
  ExtractImageWithRemote(CreateAndBindImageExtractorRemote(web_contents),
                         std::move(callback));
}

}  // namespace wallet
