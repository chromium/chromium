// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_CONTENT_EXTRACTION_SERVICE_H_
#define CHROMEOS_COMPONENTS_MAHI_CONTENT_EXTRACTION_SERVICE_H_

#include <memory>

#include "chromeos/components/mahi/ax_tree_extractor.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace mahi {

// The mahi content extraction service hosting a11y tree processes in utility
// process.
class ContentExtractionService : public mojom::ContentExtractionServiceFactory,
                                 public mojom::ContentExtractionService {
 public:
  explicit ContentExtractionService(
      mojo::PendingReceiver<mojom::ContentExtractionServiceFactory> receiver);
  ContentExtractionService(const ContentExtractionService&) = delete;
  ContentExtractionService& operator=(ContentExtractionService&) = delete;
  ~ContentExtractionService() override;

  // mojom::ContentExtractionServiceFactory:
  void BindContentExtractionService(
      mojo::PendingReceiver<mojom::ContentExtractionService> receiver) override;
  void OnScreen2xReady(
      mojo::PendingRemote<::screen_ai::mojom::Screen2xMainContentExtractor>
          screen2x_content_extractor) override;

  // mojom::ContentExtractionService:
  void ExtractContent(mojom::ExtractionRequestPtr request,
                      ExtractContentCallback callback) override;
  void GetContentSize(mojom::ExtractionRequestPtr request,
                      GetContentSizeCallback callback) override;

 private:
  mojo::Receiver<mojom::ContentExtractionServiceFactory> factory_receiver_;
  mojo::ReceiverSet<mojom::ContentExtractionService> service_receivers_;

  std::unique_ptr<AXTreeExtractor> extractor_;
};

}  // namespace mahi

#endif  // CHROMEOS_COMPONENTS_MAHI_CONTENT_EXTRACTION_SERVICE_H_
