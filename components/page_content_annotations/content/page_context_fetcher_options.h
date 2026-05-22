// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_OPTIONS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_OPTIONS_H_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/compiler_specific.h"
#include "base/types/optional_ref.h"
#include "components/page_content_annotations/core/page_content_annotations_enums.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkColor.h"

namespace page_content_annotations {

struct PaintPreviewOptions {
  // The maximum memory/file bytes used for the capture of a single frame.
  // 0 means no limit.
  size_t max_per_capture_bytes = 0;

  // Whether iframe redaction is enabled, and which scope is used if so.
  ScreenshotIframeRedactionScope iframe_redaction_scope =
      ScreenshotIframeRedactionScope::kNone;
};

class ScreenshotOptions {
 public:
  // The format of the screenshot.
  // The default value is JPEG.
  enum class ScreenshotImageFormat {
    kJpeg,
    kPng,
    kWebp,
  };

  // The compression quality of the screenshot.
  // Depending on screenshot format, the compression quality may not be
  // respected or may mean something different.
  // The default value is MEDIUM.
  enum class ScreenshotCompressionQuality {
    kLow,
    kMedium,
    kHigh,
    kNone,
  };

  struct ScreenshotCollectionOptions {
   public:
    // The maximum width of the screenshot. If 0, the screenshot will be
    // returned at the natural width of the screen. Screenshot will be scaled to
    // fit the max width and height while maintaining the aspect ratio.
    std::optional<int32_t> max_width;
    // The maximum height of the screenshot. If 0, the screenshot will be
    // returned at the natural height of the screen.
    // Screenshot will be scaled to fit the max width and height while
    // maintaining the aspect ratio.
    std::optional<int32_t> max_height;
    // The format of the screenshot.
    // The default value is JPEG.
    std::optional<ScreenshotImageFormat> screenshot_image_format;
    // The compression quality of the screenshot.
    // Depending on screenshot format, the compression quality may not be
    // respected or may mean something different.
    // The default value is MEDIUM.
    std::optional<ScreenshotCompressionQuality> screenshot_compression_quality;
  };

  // Creates options for a full-page screenshot.
  // Full-page screenshots always use the paint preview backend.
  static ScreenshotOptions FullPage(
      PaintPreviewOptions paint_preview_options,
      std::optional<ScreenshotCollectionOptions> screenshot_collection_options);

  // Creates options for a viewport-only screenshot.
  static ScreenshotOptions ViewportOnly(
      std::optional<PaintPreviewOptions> paint_preview_options,
      std::optional<ScreenshotCollectionOptions> screenshot_collection_options);

  bool capture_full_page() const { return capture_full_page_; }
  bool use_paint_preview() const { return paint_preview_options_.has_value(); }
  base::optional_ref<const PaintPreviewOptions> paint_preview_options() const
      LIFETIME_BOUND {
    return paint_preview_options_;
  }
  SkColor4f redaction_color() const { return redaction_color_; }
  void set_redaction_color_for_testing(SkColor4f redaction_color) {
    redaction_color_ = redaction_color;
  }
  const std::optional<ScreenshotCollectionOptions>&
  screenshot_collection_options() const {
    return screenshot_collection_options_;
  }

 private:
  // Private constructor to force object creation through static methods.
  ScreenshotOptions(
      bool capture_full_page,
      std::optional<PaintPreviewOptions> paint_preview_options,
      std::optional<ScreenshotCollectionOptions> screenshot_collection_options);

  // Whether to capture a full-page screenshot. If false, only the viewport will
  // be captured.
  bool capture_full_page_ = false;
  // This field must be set if capture_full_page_ is true.
  std::optional<PaintPreviewOptions> paint_preview_options_ = std::nullopt;
  // The color to paint for redaction.
  SkColor4f redaction_color_ = SkColors::kBlack;
  // Options for screenshot collection. If not set, the screenshot will be
  // captured with the default options.
  std::optional<ScreenshotCollectionOptions> screenshot_collection_options_;
};

class PdfOptions {
 public:
  enum class Format {
    kBytes,
    kText,
  };

  PdfOptions(Format format, uint32_t size_limit);

  Format format() const { return format_; }
  uint32_t size_limit() const { return size_limit_; }

 private:
  // The requested content extraction format of PDF content.
  Format format_;

  // Limit defining the number of bytes or chars for PDF contents that should be
  // returned. It must be greater than 0.
  uint32_t size_limit_;
};

struct FetchPageContextOptions {
  FetchPageContextOptions();
  ~FetchPageContextOptions();

  // Limit defining the number of bytes for inner text returned. A value
  // of 0 indicates no inner text should be returned.
  uint32_t inner_text_bytes_limit = 0;

  // Options for taking a screenshot. If not set, no screenshot will be taken.
  std::optional<ScreenshotOptions> screenshot_options = std::nullopt;

  // Options for extracting contents from a PDF document. If not set, no
  // extraction will take place for PDF.
  std::optional<PdfOptions> pdf_options = std::nullopt;

  blink::mojom::AIPageContentOptionsPtr annotated_page_content_options;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_OPTIONS_H_
