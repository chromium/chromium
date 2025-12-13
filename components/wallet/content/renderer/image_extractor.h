// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_RENDERER_IMAGE_EXTRACTOR_H_
#define COMPONENTS_WALLET_CONTENT_RENDERER_IMAGE_EXTRACTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/wallet/content/common/mojom/image_extractor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace wallet {

// Extracts images from a page for a given RenderFrame.
//
// This class is created for a `RenderFrame` and attaches a Mojo receiver to
// handle requests from the browser process. Its lifetime is tied to the
// `RenderFrame` by being stored as `UserData`.
//
// It currently extracts images from `<img>` elements only and does not extract
// images from other sources.
class ImageExtractor : public base::SupportsUserData::Data,
                       public wallet::mojom::ImageExtractor {
 public:
  // Creates an ImageExtractor and attaches it to the RenderFrame.
  static void Create(content::RenderFrame* render_frame,
                     service_manager::BinderRegistry* registry);

  ImageExtractor(const ImageExtractor&) = delete;
  ImageExtractor& operator=(const ImageExtractor&) = delete;

  ~ImageExtractor() override;

  // wallet::mojom::ImageExtractor:
  void ExtractImages(ExtractImagesCallback callback) override;

 private:
  explicit ImageExtractor(content::RenderFrame* render_frame,
                          service_manager::BinderRegistry* registry);

  // Method to bind the Mojo receiver.
  void BindReceiver(
      mojo::PendingReceiver<wallet::mojom::ImageExtractor> receiver);

  std::vector<SkBitmap> ExtractQualifiedImageElements(
      const blink::WebDocument& document) const;

  // Checks if an image is qualified for barcode detection.
  bool IsImageQualified(const SkBitmap& bitmap) const;

  // The RenderFrame to which this ImageExtractor is associated.
  raw_ptr<content::RenderFrame> render_frame_;

  mojo::Receiver<wallet::mojom::ImageExtractor> receiver_{this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_RENDERER_IMAGE_EXTRACTOR_H_
