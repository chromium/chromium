// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/content_extraction_service.h"
#include "chromeos/components/mahi/ax_tree_extractor.h"

namespace mahi {

ContentExtractionService::ContentExtractionService(
    mojo::PendingReceiver<mojom::ContentExtractionServiceFactory> receiver)
    : factory_receiver_(this, std::move(receiver)) {
  extractor_ = std::make_unique<AXTreeExtractor>();
}

ContentExtractionService::~ContentExtractionService() = default;

void ContentExtractionService::BindContentExtractionService(
    mojo::PendingReceiver<mojom::ContentExtractionService> receiver) {
  service_receivers_.Add(this, std::move(receiver));
}

void ContentExtractionService::OnScreen2xReady(
    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
        screen2x_content_extractor) {
  CHECK(screen2x_content_extractor);
  extractor_->OnScreen2xReady(std::move(screen2x_content_extractor));
}

void ContentExtractionService::ExtractContent(
    mojom::ExtractionRequestPtr request,
    ExtractContentCallback callback) {
  extractor_->ExtractContent(std::move(request), std::move(callback));
}

void ContentExtractionService::GetContentSize(
    mojom::ExtractionRequestPtr request,
    GetContentSizeCallback callback) {
  extractor_->GetContentSize(std::move(request), std::move(callback));
}

}  // namespace mahi
