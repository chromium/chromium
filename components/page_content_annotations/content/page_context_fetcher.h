// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_enums.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {

class PageContentScreenshotService;

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
  static ScreenshotOptions FullPage(PaintPreviewOptions paint_preview_options,
                                    std::optional<ScreenshotCollectionOptions>
                                        screenshot_collection_options) {
    return ScreenshotOptions(/*capture_full_page=*/true, paint_preview_options,
                             std::move(screenshot_collection_options));
  }

  // Creates options for a viewport-only screenshot.
  static ScreenshotOptions ViewportOnly(
      std::optional<PaintPreviewOptions> paint_preview_options,
      std::optional<ScreenshotCollectionOptions>
          screenshot_collection_options) {
    return ScreenshotOptions(/*capture_full_page=*/false,
                             std::move(paint_preview_options),
                             std::move(screenshot_collection_options));
  }

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
      std::optional<ScreenshotCollectionOptions> screenshot_collection_options)
      : capture_full_page_(capture_full_page),
        paint_preview_options_(paint_preview_options),
        screenshot_collection_options_(
            std::move(screenshot_collection_options)) {}

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

// TODO(b/504577535): Support PDF bookmark extraction.
// TODO(b/504577256): Support PDF accessibility info extraction.
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

// TODO(b/504577535): Support PDF bookmark extraction.
// TODO(b/504577256): Support PDF accessibility info extraction.
struct PdfResult {
  explicit PdfResult(url::Origin origin);
  PdfResult(url::Origin origin, std::vector<uint8_t> bytes);
  PdfResult(url::Origin origin, std::string text);
  PdfResult(const PdfResult&) = delete;
  PdfResult& operator=(const PdfResult&) = delete;
  PdfResult(PdfResult&&);
  PdfResult& operator=(PdfResult&&);
  ~PdfResult();

  url::Origin origin;

  // The PDF extraction result can be either bytes or string, depending on which
  // extraction option is selected.
  std::variant<std::vector<uint8_t>, std::string> data;

  bool size_exceeded = false;
};

struct ScreenshotResult {
  explicit ScreenshotResult(gfx::Size dimensions);
  ScreenshotResult(const ScreenshotResult&) = delete;
  ScreenshotResult& operator=(const ScreenshotResult&) = delete;
  ScreenshotResult(ScreenshotResult&&);
  ScreenshotResult& operator=(ScreenshotResult&&);
  ~ScreenshotResult();
  std::vector<uint8_t> screenshot_data;
  std::string mime_type;
  gfx::Size dimensions;
  base::TimeTicks end_time;
};

struct InnerTextResultWithTruncation
    : public content_extraction::InnerTextResult {
  InnerTextResultWithTruncation(std::string inner_text,
                                std::optional<unsigned> node_offset,
                                bool truncated);
  ~InnerTextResultWithTruncation();
  bool truncated = false;
};

struct PageContentResultWithEndTime
    : public optimization_guide::AIPageContentResult {
  explicit PageContentResultWithEndTime(
      optimization_guide::AIPageContentResult&& result);
  base::TimeTicks end_time;
};

struct FetchPageContextResult {
  FetchPageContextResult();
  FetchPageContextResult(const FetchPageContextResult&) = delete;
  FetchPageContextResult& operator=(const FetchPageContextResult&) = delete;
  FetchPageContextResult(FetchPageContextResult&&);
  FetchPageContextResult& operator=(FetchPageContextResult&&);
  ~FetchPageContextResult();
  base::expected<ScreenshotResult, std::string> screenshot_result;
  std::optional<InnerTextResultWithTruncation> inner_text_result;
  std::optional<PdfResult> pdf_result;
  base::expected<PageContentResultWithEndTime, std::string>
      annotated_page_content_result;
};

enum class FetchPageContextError {
  kUnknown,
  kWebContentsChanged,
  // The context is not eligible for sharing.
  kPageContextNotEligible,
  kWebContentsWentAway,
};

std::string ToString(FetchPageContextError error);

// TODO(bokan): message is redundant with error_code. Replace usage with
// ToString.
struct FetchPageContextErrorDetails {
  FetchPageContextError error_code = FetchPageContextError::kUnknown;
  std::string message;
};
using FetchPageContextResultCallbackArg =
    base::expected<std::unique_ptr<FetchPageContextResult>,
                   FetchPageContextErrorDetails>;

// Controls scaling and quality of tab screenshots.
// Does not override screenshot_collection_options if they are set, only
// modifies the default values.
BASE_DECLARE_FEATURE(kGlicTabScreenshotExperiment);

// Controls whether password fields are redacted from screenshots.
BASE_DECLARE_FEATURE(kGlicScreenshotPasswordRedaction);

// Controls whether sensitive payment fields are redacted from screenshots.
BASE_DECLARE_FEATURE(kGlicScreenshotSensitivePaymentRedaction);

extern const base::FeatureParam<int> kMaxScreenshotWidthParam;

extern const base::FeatureParam<int> kMaxScreenshotHeightParam;

extern const base::FeatureParam<int> kScreenshotQuality;

extern const base::FeatureParam<std::string> kScreenshotImageType;

extern const base::FeatureParam<base::TimeDelta> kScreenshotTimeout;

extern const base::FeatureParam<base::TimeDelta>
    kScreenshotTimeoutBrowserAllowance;

// Callback used for relaying progress.
class FetchPageProgressListener {
 public:
  virtual ~FetchPageProgressListener() = default;
  virtual void BeginScreenshot() = 0;
  virtual void ScreenshotCaptured(const SkBitmap& bitmap) = 0;
  virtual void ScreenshotRedacted(const SkBitmap& bitmap) = 0;
  virtual void EndScreenshot(std::optional<std::string> error) = 0;
  virtual void BeginAPC() = 0;
  virtual void EndAPC(std::optional<std::string> error) = 0;
};

using FetchPageContextResultCallback =
    base::OnceCallback<void(FetchPageContextResultCallbackArg)>;

using GetScreenshotServiceCallback =
    base::RepeatingCallback<PageContentScreenshotService*(
        content::BrowserContext*)>;

// Encodes a screenshot according to the enabled feature flags.
std::optional<std::vector<uint8_t>> EncodeScreenshot(const SkBitmap& bitmap,
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options);

// Coordinates fetching multiple types of page context.
class PageContextFetcher : public content::WebContentsObserver {
 public:
  explicit PageContextFetcher(
      GetScreenshotServiceCallback get_screenshot_service_callback,
      std::unique_ptr<FetchPageProgressListener> progress_listener);
  ~PageContextFetcher() override;

  void FetchStart(content::WebContents& aweb_contents,
                  const FetchPageContextOptions& options,
                  FetchPageContextResultCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(PageContextFetcherTest,
                           RedactScreenshotOnWorkerThread);
  FRIEND_TEST_ALL_PREFIXES(PageContextFetcherTest,
                           RedactScreenshotOnWorkerThreadNoRedaction);

  // Redacts a screenshot by painting over sensitive regions with
  // `redaction_color`.
  static base::expected<SkBitmap, std::string> RedactScreenshotOnWorkerThread(
      const SkBitmap& bitmap,
      const std::vector<gfx::Rect>& visible_bounding_boxes_for_redaction,
      SkColor4f redaction_color);

#if BUILDFLAG(ENABLE_PDF)
  void FetchPdfContent(const PdfOptions& options);
  void ReceivedPdfBytes(url::Origin pdf_origin,
                        uint32_t pdf_size_limit,
                        pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count);
  void ReceivedPdfText(url::Origin pdf_origin,
                       uint32_t text_byte_limit,
                       const std::u16string& text);
#endif  // BUILDFLAG(ENABLE_PDF)

  void GetTabScreenshot(content::WebContents& web_contents,
                        const ScreenshotOptions& screenshot_options);

  void SetCaptureCountLock(content::WebContents& web_contents);

  void ScheduleScreenshotTimeout();

  void ReceivedViewportBitmap(const content::CopyFromSurfaceResult& result);

  void RedactAndEncodeScreenshot(
      std::vector<gfx::Rect> visible_bounding_boxes_for_redaction);

  void RedactAndEncodeScreenshotIfNeeded();

  void ReceivedViewportBitmapOrError(
      base::expected<const SkBitmap*, std::string> bitmap_result);

  // content::WebContentsObserver impl.
  void PrimaryPageChanged(content::Page& page) override;

  void OnScreenshotTimeout();

  void ReceivedEncodedScreenshot(
      base::expected<std::pair<std::vector<uint8_t>, SkBitmap>, std::string>
          screenshot_data);

  void ReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void ReceivedAnnotatedPageContent(
      optimization_guide::AIPageContentResultOrError content);

  void RunCallbackIfComplete();

  base::WeakPtr<PageContextFetcher> GetWeakPtr();

  const GetScreenshotServiceCallback get_screenshot_service_callback_;
  FetchPageContextResultCallback callback_;

  uint32_t inner_text_bytes_limit_ = 0;

  // screenshot processing dependencies.
  std::optional<SkBitmap> screenshot_bitmap_;
  bool screenshot_needs_redaction_ = false;

  // Intermediate results:

  // Whether work is complete for each task, does not imply success.
  bool initialization_done_ = false;
  bool screenshot_capture_done_ = false;
  bool screenshot_done_ = false;
  bool inner_text_done_ = false;
  bool pdf_done_ = false;
  bool annotated_page_content_done_ = false;
  SkColor4f screenshot_redaction_color_ = SkColors::kBlack;
  std::optional<ScreenshotOptions::ScreenshotCollectionOptions>
      screenshot_collection_options_;
  // Whether the primary page has changed since context fetching began.
  bool primary_page_changed_ = false;
  std::unique_ptr<FetchPageContextResult> pending_result_;
  base::ElapsedTimer elapsed_timer_;
  base::ScopedClosureRunner capture_count_lock_;

  std::unique_ptr<FetchPageProgressListener> progress_listener_;

  base::WeakPtrFactory<PageContextFetcher> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_H_
