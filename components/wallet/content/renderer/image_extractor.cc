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

// The maximum number of images to extract from a page.
// TODO(crbug.com/445386472): Use finch params to control.
constexpr size_t kMaxImages = 10;

// The minimum height and width of an image to be considered qualified for
// barcode detection.
// TODO(crbug.com/445386472): Use finch params to control.
constexpr int kMinImageSize = 10;

// The maximum aspect ratio of an image to be considered qualified for barcode
// detection.
// TODO(crbug.com/445386472): Use finch params to control.
constexpr int kMaxAspectRatio = 15;

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
  std::move(callback).Run(ExtractQualifiedImageElements(doc));
}

void ImageExtractor::BindReceiver(
    mojo::PendingReceiver<wallet::mojom::ImageExtractor> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

// Extract all <img> elements from the document
std::vector<SkBitmap> ImageExtractor::ExtractQualifiedImageElements(
    const blink::WebDocument& document) const {
  blink::WebElementCollection image_elements =
      document.GetElementsByHTMLTagName("img");

  std::vector<SkBitmap> skia_images;
  skia_images.reserve(image_elements.length());
  for (blink::WebElement element = image_elements.FirstItem();
       !element.IsNull(); element = image_elements.NextItem()) {
    if (skia_images.size() >= kMaxImages) {
      break;
    }
    SkBitmap skia_image = element.ImageContents();
    if (IsImageQualified(skia_image)) {
      skia_images.push_back(std::move(skia_image));
    }
  }
  return skia_images;
}

// Checks if an image is qualified for barcode detection. An image is
// considered qualified if it is not empty, meets minimum size requirements,
// and does not have an extreme aspect ratio.
bool ImageExtractor::IsImageQualified(const SkBitmap& bitmap) const {
  // Empty images are not qualified.
  if (bitmap.empty()) {
    return false;
  }
  // Images that are too small are not qualified.
  if (std::min(bitmap.height(), bitmap.width()) < kMinImageSize) {
    return false;
  }
  // Images with extreme aspect ratios are not qualified.
  const int larger_dim = std::max(bitmap.width(), bitmap.height());
  const int smaller_dim = std::min(bitmap.width(), bitmap.height());
  if (static_cast<double>(larger_dim) >
      static_cast<double>(smaller_dim) * kMaxAspectRatio) {
    return false;
  }

  return true;
}

}  // namespace wallet
