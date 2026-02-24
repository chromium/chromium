// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "pdf/mojom/pdf.mojom.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {

class PageContentScreenshotService;

enum class ScreenshotIframeRedactionScope {
  // No redaction.
  kNone,
  // Redact cross-site iframes.
  kCrossSite,
  // Redact cross-origin iframes.
  kCrossOrigin,
};

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
  // Creates options for a full-page screenshot.
  // Full-page screenshots always use the paint preview backend.
  static ScreenshotOptions FullPage(PaintPreviewOptions paint_preview_options) {
    return ScreenshotOptions(/*capture_full_page=*/true, paint_preview_options);
  }

  // Creates options for a viewport-only screenshot.
  static ScreenshotOptions ViewportOnly(
      std::optional<PaintPreviewOptions> paint_preview_options) {
    return ScreenshotOptions(/*capture_full_page=*/false,
                             std::move(paint_preview_options));
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

 private:
  // Private constructor to force object creation through static methods.
  ScreenshotOptions(bool capture_full_page,
                    std::optional<PaintPreviewOptions> paint_preview_options)
      : capture_full_page_(capture_full_page),
        paint_preview_options_(paint_preview_options) {}

  // Whether to capture a full-page screenshot. If false, only the viewport will
  // be captured.
  bool capture_full_page_ = false;
  // This field must be set if capture_full_page_ is true.
  std::optional<PaintPreviewOptions> paint_preview_options_ = std::nullopt;
  // The color to paint for redaction.
  SkColor4f redaction_color_ = SkColors::kBlack;
};

struct FetchPageContextOptions {
  FetchPageContextOptions();
  ~FetchPageContextOptions();

  // Limit defining the number of bytes for inner text returned. A value
  // of 0 indicates no inner text should be returned.
  uint32_t inner_text_bytes_limit = 0;

  // Options for taking a screenshot. If not set, no screenshot will be taken.
  std::optional<ScreenshotOptions> screenshot_options = std::nullopt;

  blink::mojom::AIPageContentOptionsPtr annotated_page_content_options;

  // Limit defining number of bytes for PDF data that should be returned.
  // A value of 0 indicates no pdf data should be returned.
  uint32_t pdf_size_limit = 0;
};

struct PdfResult {
  explicit PdfResult(url::Origin origin);
  PdfResult(url::Origin origin, std::vector<uint8_t> bytes);
  ~PdfResult();
  url::Origin origin;
  std::vector<uint8_t> bytes;
  bool size_exceeded = false;
};

struct ScreenshotResult {
  explicit ScreenshotResult(gfx::Size dimensions);
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

// Callback used for relaying progress.
class FetchPageProgressListener {
 public:
  virtual ~FetchPageProgressListener() = default;
  virtual void BeginScreenshot() {}
  virtual void EndScreenshot(std::optional<std::string> error) {}
  virtual void BeginAPC() {}
  virtual void EndAPC(std::optional<std::string> error) {}
};

using FetchPageContextResultCallback =
    base::OnceCallback<void(FetchPageContextResultCallbackArg)>;

using GetScreenshotServiceCallback =
    base::RepeatingCallback<PageContentScreenshotService*(
        content::BrowserContext*)>;

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
#if !BUILDFLAG(IS_ANDROID)
  void ReceivedPdfBytes(const url::Origin& pdf_origin,
                        uint32_t pdf_size_limit,
                        pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count);
#endif

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
      base::expected<std::vector<uint8_t>, std::string> screenshot_data);

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
  bool screenshot_needs_password_redaction_ = false;
  bool screenshot_needs_sensitive_payment_redaction_ = false;

  // Intermediate results:

  // Whether work is complete for each task, does not imply success.
  bool initialization_done_ = false;
  bool screenshot_capture_done_ = false;
  bool screenshot_done_ = false;
  bool inner_text_done_ = false;
  bool pdf_done_ = false;
  bool annotated_page_content_done_ = false;
  SkColor4f screenshot_redaction_color_ = SkColors::kBlack;
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
