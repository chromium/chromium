// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/renderer/image_extractor.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

// Key used to associate ImageExtractor with a RenderFrame.
const void* const kUserDataKey = &kUserDataKey;

}  // namespace

namespace wallet {

ImageExtractor::ImageExtractor(content::RenderFrame* render_frame,
                               service_manager::BinderRegistry* registry)
    : render_frame_(render_frame) {
  // Unretained is safe here because `registry` is also scoped to the
  // RenderFrame and the ImageExtractor's lifetime is tied to the RenderFrame.
  registry->AddInterface(base::BindRepeating(&ImageExtractor::BindReceiver,
                                             base::Unretained(this)));
}

ImageExtractor::~ImageExtractor() = default;

// static
void ImageExtractor::Create(content::RenderFrame* render_frame,
                            service_manager::BinderRegistry* registry) {
  // Do nothing if an ImageExtractor is already attached to the RenderFrame.
  if (render_frame->GetUserData(kUserDataKey)) {
    return;
  }

  render_frame->SetUserData(kUserDataKey, base::WrapUnique(new ImageExtractor(
                                              render_frame, registry)));
}

void ImageExtractor::ExtractImages(ExtractImagesCallback callback) {
  blink::WebDocument doc = render_frame_->GetWebFrame()->GetDocument();
  std::move(callback).Run(ExtractImageElements(doc));
}

void ImageExtractor::BindReceiver(
    mojo::PendingReceiver<wallet::mojom::ImageExtractor> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

// Extract all <img> elements from the document
std::vector<SkBitmap> ImageExtractor::ExtractImageElements(
    const blink::WebDocument& document) {
  blink::WebElementCollection image_elements =
      document.GetElementsByHTMLTagName("img");

  // TODO(crbug.com/438617323): Implement image filtering and number limit.
  std::vector<SkBitmap> skia_images;
  skia_images.reserve(image_elements.length());
  for (blink::WebElement element = image_elements.FirstItem();
       !element.IsNull(); element = image_elements.NextItem()) {
    SkBitmap skia_image = element.ImageContents();
    if (!skia_image.isNull() && !skia_image.empty()) {
      skia_images.push_back(std::move(skia_image));
    }
  }
  return skia_images;
}

}  // namespace wallet
