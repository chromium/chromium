// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"

#include "base/functional/callback.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

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

mojo::Remote<mojom::ImageExtractor>
WalletablePassBarcodeDetectorImpl::CreateAndBindImageExtractorRemote(
    content::WebContents* web_contents) {
  DCHECK(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  mojo::Remote<mojom::ImageExtractor> remote;
  web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      remote.BindNewPipeAndPassReceiver());
  return remote;
}

void WalletablePassBarcodeDetectorImpl::ExtractImageWithRemote(
    mojo::Remote<mojom::ImageExtractor> remote,
    WalletBarcodeDetectionDetectCallback callback) {
  if (!remote || image_extractor_remote_) {
    std::move(callback).Run({});
    return;
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

  if (images.empty()) {
    std::move(callback).Run({});
    return;
  }

  // TODO(crbug.com/438364540): Implement the actual detection logic here.
  std::move(callback).Run({});
}

void WalletablePassBarcodeDetectorImpl::Detect(
    content::WebContents* web_contents,
    WalletBarcodeDetectionDetectCallback callback) {
  ExtractImageWithRemote(CreateAndBindImageExtractorRemote(web_contents),
                         std::move(callback));
}

}  // namespace wallet
