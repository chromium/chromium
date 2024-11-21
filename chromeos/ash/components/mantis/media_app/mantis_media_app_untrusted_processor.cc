// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"

#include <utility>

#include "chromeos/ash/components/mantis/mojom/mantis_processor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

MantisMediaAppUntrustedProcessor::MantisMediaAppUntrustedProcessor()
    : receiver_(this) {}

MantisMediaAppUntrustedProcessor::~MantisMediaAppUntrustedProcessor() = default;

mojo::PendingReceiver<mantis::mojom::MantisProcessor>
MantisMediaAppUntrustedProcessor::BindNewPipeAndPassReceiver() {
  return processor_.BindNewPipeAndPassReceiver();
}

mojo::PendingRemote<media_app_ui::mojom::MantisMediaAppUntrustedProcessor>
MantisMediaAppUntrustedProcessor::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MantisMediaAppUntrustedProcessor::SegmentImage(
    const std::vector<uint8_t>& image,
    const std::vector<uint8_t>& selection,
    SegmentImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->Segmentation(image, selection, std::move(callback));
}

void MantisMediaAppUntrustedProcessor::GenerativeFillImage(
    const std::vector<uint8_t>& image,
    const std::vector<uint8_t>& mask,
    const std::string& text,
    uint32_t seed,
    GenerativeFillImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->GenerativeFill(image, mask, seed, text, std::move(callback));
}

void MantisMediaAppUntrustedProcessor::InpaintImage(
    const std::vector<uint8_t>& image,
    const std::vector<uint8_t>& mask,
    uint32_t seed,
    InpaintImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->Inpainting(image, mask, seed, std::move(callback));
}

void MantisMediaAppUntrustedProcessor::ClassifyImageSafety(
    const std::vector<uint8_t>& image,
    ClassifyImageSafetyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  processor_->ClassifyImageSafety(image, std::move(callback));
}

}  // namespace ash
