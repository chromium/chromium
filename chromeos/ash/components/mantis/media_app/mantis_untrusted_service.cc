// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service.h"

#include <utility>

#include "chromeos/ash/components/mantis/mojom/mantis_processor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

MantisUntrustedService::MantisUntrustedService(
    mojo::PendingRemote<mantis::mojom::MantisProcessor> processor)
    : receiver_(this), processor_(std::move(processor)) {}

MantisUntrustedService::~MantisUntrustedService() = default;

mojo::PendingRemote<media_app_ui::mojom::MantisUntrustedService>
MantisUntrustedService::BindNewPipeAndPassRemote(
    base::OnceClosure disconnect_handler) {
  mojo::PendingRemote<media_app_ui::mojom::MantisUntrustedService> remote =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
  return remote;
}

void MantisUntrustedService::SegmentImage(const std::vector<uint8_t>& image,
                                          const std::vector<uint8_t>& selection,
                                          SegmentImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->Segmentation(image, selection, std::move(callback));
}

void MantisUntrustedService::GenerativeFillImage(
    const std::vector<uint8_t>& image,
    const std::vector<uint8_t>& mask,
    const std::string& text,
    uint32_t seed,
    GenerativeFillImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->GenerativeFill(image, mask, seed, text, std::move(callback));
}

void MantisUntrustedService::InpaintImage(const std::vector<uint8_t>& image,
                                          const std::vector<uint8_t>& mask,
                                          uint32_t seed,
                                          InpaintImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->Inpainting(image, mask, seed, std::move(callback));
}

void MantisUntrustedService::OutpaintImage(const std::vector<uint8_t>& image,
                                           const std::vector<uint8_t>& mask,
                                           uint32_t seed,
                                           OutpaintImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->Outpainting(image, mask, seed, std::move(callback));
}

void MantisUntrustedService::ClassifyImageSafety(
    const std::vector<uint8_t>& image,
    ClassifyImageSafetyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->ClassifyImageSafety(image, std::move(callback));
}

void MantisUntrustedService::InferSegmentationMode(
    std::vector<mantis::mojom::TouchPointPtr> gesture,
    InferSegmentationModeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->InferSegmentationMode(std::move(gesture), std::move(callback));
}

}  // namespace ash
