// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher_options.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/check_op.h"

namespace page_content_annotations {

// static
ScreenshotOptions ScreenshotOptions::FullPage(
    PaintPreviewOptions paint_preview_options,
    std::optional<ScreenshotCollectionOptions> screenshot_collection_options) {
  return ScreenshotOptions(/*capture_full_page=*/true, paint_preview_options,
                           std::move(screenshot_collection_options));
}

// static
ScreenshotOptions ScreenshotOptions::ViewportOnly(
    std::optional<PaintPreviewOptions> paint_preview_options,
    std::optional<ScreenshotCollectionOptions> screenshot_collection_options) {
  return ScreenshotOptions(/*capture_full_page=*/false,
                           std::move(paint_preview_options),
                           std::move(screenshot_collection_options));
}

ScreenshotOptions::ScreenshotOptions(
    bool capture_full_page,
    std::optional<PaintPreviewOptions> paint_preview_options,
    std::optional<ScreenshotCollectionOptions> screenshot_collection_options)
    : capture_full_page_(capture_full_page),
      paint_preview_options_(paint_preview_options),
      screenshot_collection_options_(std::move(screenshot_collection_options)) {
}

PdfOptions::PdfOptions(Format format, uint32_t size_limit)
    : format_(format), size_limit_(size_limit) {
  CHECK_GT(size_limit, 0u);
}

FetchPageContextOptions::FetchPageContextOptions() = default;

FetchPageContextOptions::~FetchPageContextOptions() = default;

}  // namespace page_content_annotations
