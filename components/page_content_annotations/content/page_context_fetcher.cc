// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/core/page_content_proto_serializer.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_screenshot_service.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/redaction_params.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/origin.h"

namespace page_content_annotations {

namespace {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPageContextFetcherAndroidViewportCrop,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

gfx::Size GetScreenshotSize(
    const gfx::Size& original_size,
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  if (original_size.IsEmpty()) {
    return gfx::Size();
  }

  // By default, no scaling.
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment) &&
      !screenshot_collection_options) {
    return gfx::Size();
  }

  // If either width or height is 0, or the view is empty, no scaling.
  int max_width = (screenshot_collection_options &&
                   screenshot_collection_options->max_width)
                      ? screenshot_collection_options->max_width.value()
                      : kMaxScreenshotWidthParam.Get();
  int max_height = (screenshot_collection_options &&
                    screenshot_collection_options->max_height)
                       ? screenshot_collection_options->max_height.value()
                       : kMaxScreenshotHeightParam.Get();
  if (max_width == 0 || max_height == 0) {
    return gfx::Size();
  }

  double aspect_ratio = static_cast<double>(original_size.width()) /
                        static_cast<double>(original_size.height());

  int new_width = original_size.width();
  int new_height = original_size.height();

  // If larger than width or height, scale down while preserving aspect
  // ratio.
  if (new_width > max_width) {
    new_width = max_width;
    new_height = static_cast<int>(max_width / aspect_ratio);
  }
  if (new_height > max_height) {
    new_height = max_height;
    new_width = static_cast<int>(max_height * aspect_ratio);
  }

  return gfx::Size(new_width, new_height);
}

double GetScreenshotScaleFactor(const gfx::Size& original_size,
                                const gfx::Size& new_size) {
  if (new_size.IsEmpty()) {
    // When the new size is empty, that means no scaling.
    return 1.0;
  }
  // The aspect ratio was preserved by GetScreenshotSize, so the ratio of the
  // new width to old width should be the same as the ratio of new height to old
  // height. WLOG, we'll use the widths.
  return static_cast<double>(new_size.width()) / original_size.width();
}

int GetScreenshotJpegQuality(
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  if (screenshot_collection_options &&
      screenshot_collection_options->screenshot_compression_quality) {
    switch (
        screenshot_collection_options->screenshot_compression_quality.value()) {
      case ScreenshotOptions::ScreenshotCompressionQuality::kLow:
        return 20;
      case ScreenshotOptions::ScreenshotCompressionQuality::kMedium:
        return 40;
      case ScreenshotOptions::ScreenshotCompressionQuality::kHigh:
        return 60;
      case ScreenshotOptions::ScreenshotCompressionQuality::kNone:
        return 100;
    }
  }
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return 40;
  }
  // Must be an int from 0 to 100.
  return std::max(0, std::min(100, kScreenshotQuality.Get()));
}

int GetScreenshotWebPQuality(
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  return GetScreenshotJpegQuality(screenshot_collection_options);
}

// Png only has two modes exposed.
bool ShouldPngScreenshotBeLowQuality(
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  // If low is configured, then we should use low quality for png screenshots.
  if (screenshot_collection_options &&
      screenshot_collection_options->screenshot_compression_quality) {
    return screenshot_collection_options->screenshot_compression_quality
               .value() ==
           ScreenshotOptions::ScreenshotCompressionQuality::kLow;
  }
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return false;
  }
  // We use the quality to determine if it is low quality or not by checking if
  // it is 50 or lower.
  return kScreenshotQuality.Get() < 50;
}

enum class ScreenshotImageType {
  kUnknown = 0,
  kJpeg = 1,
  kPng = 2,
  kWebp = 3,
  kMaxValue = kWebp,
};

// We use a timer on the viz side as well as a timer on the browser side and
// offset them by this allowance. Hopefully having the viz timer fire always
// as this will give us more information as to the failure.

ScreenshotImageType GetScreenshotImageType(
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  if (screenshot_collection_options &&
      screenshot_collection_options->screenshot_image_format) {
    switch (screenshot_collection_options->screenshot_image_format.value()) {
      case ScreenshotOptions::ScreenshotImageFormat::kJpeg:
        return ScreenshotImageType::kJpeg;
      case ScreenshotOptions::ScreenshotImageFormat::kPng:
        return ScreenshotImageType::kPng;
      case ScreenshotOptions::ScreenshotImageFormat::kWebp:
        return ScreenshotImageType::kWebp;
    }
  }

  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return ScreenshotImageType::kJpeg;
  }
  if (kScreenshotImageType.Get() == "jpeg") {
    return ScreenshotImageType::kJpeg;
  }
  if (kScreenshotImageType.Get() == "png") {
    return ScreenshotImageType::kPng;
  }
  if (kScreenshotImageType.Get() == "webp") {
    return ScreenshotImageType::kWebp;
  }
  return ScreenshotImageType::kJpeg;
}

base::expected<paint_preview::RedactionParams, std::string> GetRedactionParams(
    content::WebContents& web_contents,
    ScreenshotIframeRedactionScope screenshot_iframe_redaction_scope) {
  auto* frame = web_contents.GetPrimaryMainFrame();
  if (!frame) {
    return base::unexpected("Could not get primary main frame.");
  }

  switch (screenshot_iframe_redaction_scope) {
    case ScreenshotIframeRedactionScope::kNone:
      return paint_preview::RedactionParams();
    case ScreenshotIframeRedactionScope::kCrossSite:
      return paint_preview::RedactionParams(
          /*allowed_origins=*/{},
          /*allowed_sites=*/{
              net::SchemefulSite(frame->GetLastCommittedOrigin())});
    case ScreenshotIframeRedactionScope::kCrossOrigin:
      return paint_preview::RedactionParams(
          /*allowed_origins=*/{frame->GetLastCommittedOrigin()},
          /*allowed_sites=*/{});
  }
  NOTREACHED();
}

std::string_view ToString(content::CopyFromSurfaceError error) {
  switch (error) {
    case content::CopyFromSurfaceError::kUnknown:
      return "Unknown";
    case content::CopyFromSurfaceError::kNotImplemented:
      return "Not implemented";
    case content::CopyFromSurfaceError::kFrameGone:
      return "Frame Gone";
    case content::CopyFromSurfaceError::kTimeout:
      return "Timeout";
    case content::CopyFromSurfaceError::kEmbeddingTokenChanged:
      return "EmbeddingTokenChanged";
    case content::CopyFromSurfaceError::kVizSentEmptyBitmap:
      return "VizSentEmptyBitmap";
    case content::CopyFromSurfaceError::kUnknownVizError:
      return "UnknownVizError";
  }
}

// Truncate the UTF-8 string to the nearest UTF-8 character that will leave the
// string size less than or equal to the byte limit. Returns true if the text
// was truncated.
bool TruncateUTF8ToByteLimit(std::string& text, size_t byte_limit) {
  const size_t truncated_size =
      base::TruncateUTF8ToByteSize(std::string_view{text}, byte_limit).size();
  if (truncated_size < text.size()) {
    text.resize(truncated_size);
    return true;
  }

  return false;
}

}  // namespace

// static
base::expected<SkBitmap, std::string>
PageContextFetcher::RedactScreenshotOnWorkerThread(
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& visible_bounding_boxes_for_redaction,
    SkColor4f redaction_color) {
  base::UmaHistogramBoolean("Glic.PageContextFetcher.ScreenshotRedacted",
                            !visible_bounding_boxes_for_redaction.empty());

  if (visible_bounding_boxes_for_redaction.empty()) {
    return bitmap;
  }

  SkBitmap redacted_bitmap;
  if (!redacted_bitmap.setInfo(bitmap.info())) {
    return base::unexpected("Failed to set info for redacted bitmap");
  }

  if (!redacted_bitmap.tryAllocPixels()) {
    return base::unexpected("Failed to allocate pixels for redacted bitmap");
  }

  if (!redacted_bitmap.writePixels(bitmap.pixmap())) {
    return base::unexpected("Failed to copy pixels for screenshot redaction");
  }

  SkCanvas canvas(redacted_bitmap);
  SkPaint paint;
  paint.setColor(redaction_color);
  for (const auto& rect : visible_bounding_boxes_for_redaction) {
    canvas.drawRect(RectToSkRect(rect), paint);
  }

  return redacted_bitmap;
}

// static
std::optional<std::vector<uint8_t>> EncodeScreenshot(
    const SkBitmap& bitmap,
    const std::optional<ScreenshotOptions::ScreenshotCollectionOptions>&
        screenshot_collection_options) {
  std::optional<std::vector<uint8_t>> encoded;
  switch (GetScreenshotImageType(screenshot_collection_options)) {
    case ScreenshotImageType::kJpeg:
      encoded = gfx::JPEGCodec::Encode(
          bitmap, GetScreenshotJpegQuality(screenshot_collection_options));
      break;
    case ScreenshotImageType::kPng:
      if (ShouldPngScreenshotBeLowQuality(screenshot_collection_options)) {
        encoded = gfx::PNGCodec::FastEncodeBGRASkBitmap(
            bitmap, /*discard_transparency=*/true);
      } else {
        encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
            bitmap, /*discard_transparency=*/true);
      }
      break;
    case ScreenshotImageType::kWebp:
      encoded = gfx::WebpCodec::Encode(
          bitmap, GetScreenshotWebPQuality(screenshot_collection_options));
      break;
    default:
      break;
  }
  return encoded;
}

PageContextFetcher::PageContextFetcher(
    GetScreenshotServiceCallback get_screenshot_service_callback,
    std::unique_ptr<FetchPageProgressListener> progress_listener,
    FetchPdfContentCallback fetch_pdf_content_callback)
    : get_screenshot_service_callback_(
          std::move(get_screenshot_service_callback)),
      fetch_pdf_content_callback_(std::move(fetch_pdf_content_callback)),
      progress_listener_(std::move(progress_listener)) {}
PageContextFetcher::~PageContextFetcher() {
  if (callback_) {
    std::move(callback_).Run(base::unexpected(FetchPageContextErrorDetails{
        FetchPageContextError::kWebContentsWentAway,
        "web contents went away (fetcher destroyed)"}));
  }
}

void PageContextFetcher::FetchStart(content::WebContents& aweb_contents,
                                    const FetchPageContextOptions& options,
                                    FetchPageContextResultCallback callback) {
  pending_result_ = std::make_unique<FetchPageContextResult>();
  DCHECK(aweb_contents.GetPrimaryMainFrame());
  CHECK_EQ(web_contents(),
           nullptr);  // Ensure Fetch is called only once per instance.
  Observe(&aweb_contents);
  // TODO(crbug.com/391851902): implement kSensitiveContentAttribute error
  // checking and signaling.
  callback_ = std::move(callback);

  if (options.screenshot_options) {
    GetTabScreenshot(*web_contents(), options.screenshot_options.value());
  } else {
    screenshot_done_ = true;
  }

  inner_text_bytes_limit_ = options.inner_text_bytes_limit;
  if (options.inner_text_bytes_limit > 0) {
    content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
    // This could be more efficient if GetInnerText
    // supported a max length. Instead, we truncate after generating the full
    // text.
    auto params = blink::mojom::InnerTextParams::New();
    params->include_edit_context = options.inner_text_include_edit_context;
    content_extraction::GetInnerTextWithParams(
        *frame, std::move(params),
        base::BindOnce(&PageContextFetcher::ReceivedInnerText, GetWeakPtr()));
  } else {
    inner_text_done_ = true;
  }

  pdf_done_ = true;  // Will not fetch PDF contents by default.
  if (options.pdf_options && options.pdf_options->size_limit() > 0 &&
      fetch_pdf_content_callback_) {
    // Set `pdf_done_` to false before running `fetch_pdf_content_callback_`,
    // because it may invoke the result callback synchronously. For example,
    // `PDFDocumentHelper::GetPdfBytes` invokes the callback immediately if it
    // finds `remote_pdf_client_` is invalid.
    pdf_done_ = false;  // Will fetch PDF contents.
    fetch_pdf_content_callback_.Run(
        *web_contents(), *(options.pdf_options),
        base::BindOnce(&PageContextFetcher::ReceivedPdfResult,
                       pdf_weak_ptr_factory_.GetWeakPtr()));
    // Schedule a timeout to prevent a hanging PDF extraction from making the
    // entire page context fetch wait for the PDF extraction result
    // indefinitely.
    if (!pdf_done_) {
      SchedulePdfExtractionTimeout();
    }
  }

  if (options.annotated_page_content_options) {
    blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
        options.annotated_page_content_options.Clone();
    ai_page_content_options->on_critical_path = true;
    if (progress_listener_) {
      progress_listener_->BeginAPC();
    }
    const bool use_tracked_elements_for_password_screenshot_redaction =
        base::FeatureList::IsEnabled(
            blink::features::kAIPageContentTrackedElementsPassword) &&
        options.screenshot_options &&
        !options.screenshot_options->use_paint_preview();
    ai_page_content_options->include_passwords_for_redaction =
        base::FeatureList::IsEnabled(kGlicScreenshotPasswordRedaction) &&
        !use_tracked_elements_for_password_screenshot_redaction;
    ai_page_content_options->include_sensitive_payments_for_redaction =
        base::FeatureList::IsEnabled(kGlicScreenshotSensitivePaymentRedaction);
    // OTP redaction reuses the shared APC Autofill feature gate because there
    // is no separate screenshot-only OTP pipeline. The browser asks APC for
    // OTP boxes through the shared Autofill gate, and APC folds them into the
    // final screenshot redaction vector.
    ai_page_content_options->include_otps_for_redaction =
        base::FeatureList::IsEnabled(
            optimization_guide::features::
                kAnnotatedPageContentAutofillOtpRedactions);
    screenshot_needs_redaction_using_apc_ =
        ai_page_content_options->include_passwords_for_redaction ||
        ai_page_content_options->include_sensitive_payments_for_redaction ||
        ai_page_content_options->include_otps_for_redaction;
    optimization_guide::GetAIPageContent(
        web_contents(), std::move(ai_page_content_options),
        base::BindOnce(&PageContextFetcher::ReceivedAnnotatedPageContent,
                       GetWeakPtr()));
  } else {
    annotated_page_content_done_ = true;
  }

  // Note: initialization_done_ guards against processing
  // `RunCallbackIfComplete()` until we reach this point.
  initialization_done_ = true;
  RunCallbackIfComplete();
}

void PageContextFetcher::SchedulePdfExtractionTimeout() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageContextFetcher::AbortPdfExtraction, GetWeakPtr()),
      kPdfExtractionTimeout.Get());
}

void PageContextFetcher::AbortPdfExtraction() {
  if (pdf_done_) {
    return;
  }

  // Cancel the pending PDF result callback, so that a late extraction result
  // is ignored by the `FetchPdfContentCallback` implementation.
  pdf_weak_ptr_factory_.InvalidateWeakPtrs();

  // PDF extraction is aborted. Continue extracting page context and treat PDF
  // extraction as complete with a null result.
  pdf_done_ = true;
  RunCallbackIfComplete();
}

void PageContextFetcher::ReceivedPdfResult(
    std::optional<PdfResult> pdf_result) {
  // This function can be called after PDF extraction has reached timeout.
  // Early return in that case.
  if (pdf_done_) {
    return;
  }
  pdf_done_ = true;
  if (pdf_result.has_value()) {
    pending_result_->pdf_result = std::move(pdf_result);
  }
  RunCallbackIfComplete();
}

void PageContextFetcher::GetTabScreenshot(
    content::WebContents& web_contents,
    const ScreenshotOptions& screenshot_options) {
  auto* view = web_contents.GetRenderWidgetHostView();
  if (progress_listener_) {
    progress_listener_->BeginScreenshot();
  }

  if (!view || !view->IsSurfaceAvailableForCopy()) {
    ReceivedEncodedScreenshot(
        base::unexpected("Could not retrieve RenderWidgetHostView."));
    return;
  }

  screenshot_redaction_color_ = screenshot_options.redaction_color();
  screenshot_collection_options_ =
      screenshot_options.screenshot_collection_options();

  gfx::Size view_size = view->GetViewBounds().size();
  float dsf = view->GetDeviceScaleFactor();
  original_view_size_pixels_ = gfx::ScaleToRoundedSize(view_size, dsf);

  if (screenshot_options.use_paint_preview()) {
    PageContentScreenshotService* service =
        get_screenshot_service_callback_.Run(web_contents.GetBrowserContext());
    if (!service) {
      ReceivedEncodedScreenshot(
          base::unexpected("Could not get PageContentScreenshotService."));
      return;
    }

    ASSIGN_OR_RETURN(
        paint_preview::RedactionParams redaction_params,
        GetRedactionParams(
            web_contents,
            screenshot_options.paint_preview_options()->iframe_redaction_scope),
        [&](std::string error) {
          ReceivedEncodedScreenshot(base::unexpected(std::move(error)));
          return;
        });

    SetCaptureCountLock(web_contents);
    ScheduleScreenshotTimeout();

    gfx::Rect clip_rect = gfx::Rect(view_size);
    paint_preview::mojom::ClipCoordOverride clip_coord_override =
        paint_preview::mojom::ClipCoordOverride::kScrollOffset;

    if (screenshot_options.capture_full_page()) {
      clip_rect = gfx::Rect();
      clip_coord_override = paint_preview::mojom::ClipCoordOverride::kNone;
      view_size = web_contents.GetPrimaryMainFrame()->GetFrameSize().value_or(
          gfx::Size());
      original_view_size_pixels_ = gfx::ScaleToRoundedSize(view_size, dsf);
    }
    PageContentScreenshotService::RequestParams request_params = {
        .clip_rect = clip_rect,
        .scale_factor = GetScreenshotScaleFactor(
            original_view_size_pixels_,
            GetScreenshotSize(original_view_size_pixels_,
                              screenshot_collection_options_)),
        .clip_x_coord_override = clip_coord_override,
        .clip_y_coord_override = clip_coord_override,
        .redaction_params = std::move(redaction_params),
        .max_per_capture_bytes =
            screenshot_options.paint_preview_options()->max_per_capture_bytes,
    };
    service->RequestScreenshot(
        &web_contents, std::move(request_params),
        base::BindOnce(&PageContextFetcher::ReceivedViewportBitmapOrError,
                       GetWeakPtr(), viz::TrackedElementRects()));
  } else {
    SetCaptureCountLock(web_contents);
    ScheduleScreenshotTimeout();

    gfx::Rect src_rect;

#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(kPageContextFetcherAndroidViewportCrop)) {
      // On Android, the captured surface may be larger than the viewport (e.g.
      // including the browser control). The view bounds represents the actual
      // web content area, excluding top/bottom controls.
      //
      // Importantly, Blink's coordinate system always renders the actual web
      // content starting at (0, 0) of the compositor surface, regardless of
      // whether the browser control is configured at the top or at the bottom.
      // This means the "extra" unrendered blank space corresponding to the
      // height of the browser controls is always appended at the bottom of the
      // compositor surface.
      //
      // Therefore, we can safely crop the screenshot to match the viewport size
      // starting at (0, 0) without any vertical offsets.
      src_rect = gfx::Rect(original_view_size_pixels_);
    }
#endif

    view->CopyFromSurface(
        src_rect,
        GetScreenshotSize(original_view_size_pixels_,
                          screenshot_collection_options_),
        kScreenshotTimeout.Get(),
        base::BindOnce(&PageContextFetcher::ReceivedViewportBitmap,
                       GetWeakPtr()));
  }
}

void PageContextFetcher::SetCaptureCountLock(
    content::WebContents& web_contents) {
  capture_count_lock_ = web_contents.IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
      /*is_activity=*/false);
}

void PageContextFetcher::ScheduleScreenshotTimeout() {
  // Fetching the screenshot sometimes hangs. Quit early if it's taking too
  // long. b/431837630.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageContextFetcher::OnScreenshotTimeout, GetWeakPtr()),
      kScreenshotTimeout.Get() + kScreenshotTimeoutBrowserAllowance.Get());
}

void PageContextFetcher::ReceivedViewportBitmap(
    const content::CopyFromSurfaceResult& result) {
  if (!result.has_value()) {
    base::UmaHistogramEnumeration("Glic.PageContextFetcher.GetScreenshotError",
                                  result.error());
    ReceivedViewportBitmapOrError(
        viz::TrackedElementRects(),
        base::unexpected<std::string>(ToString(result.error())));
    return;
  }

  ReceivedViewportBitmapOrError(result->tracked_element_rects,
                                base::ok(&result->bitmap));
}

void PageContextFetcher::ReceivedViewportBitmapOrError(
    const viz::TrackedElementRects& tracked_element_rects,
    base::expected<const SkBitmap*, std::string> bitmap_result) {
  // Early exit if the timeout has fired.
  if (screenshot_done_) {
    return;
  }
  if (bitmap_result.has_value()) {
    const SkBitmap* bitmap = bitmap_result.value();
    pending_result_->screenshot_result.emplace(
        gfx::SkISizeToSize(bitmap->dimensions()));
    screenshot_bitmap_ = *bitmap;
    screenshot_capture_done_ = true;
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetScreenshot",
                            elapsed_timer_.Elapsed());
    if (progress_listener_) {
      progress_listener_->ScreenshotCaptured(*bitmap);
    }
    ProcessTrackedElementRects(tracked_element_rects);
    MaybeAddIframeInfo();
    RedactAndEncodeScreenshotIfNeeded();
  } else {
    ReceivedEncodedScreenshot(base::unexpected(bitmap_result.error()));
  }
}

void PageContextFetcher::RedactAndEncodeScreenshot(
    std::vector<gfx::Rect> visible_bounding_boxes_for_redaction) {
  CHECK(screenshot_bitmap_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const SkBitmap& bitmap,
             std::vector<gfx::Rect> visible_bounding_boxes_for_redaction,
             SkColor4f redaction_color,
             std::optional<ScreenshotOptions::ScreenshotCollectionOptions>
                 screenshot_collection_options)
              -> base::expected<std::pair<std::vector<uint8_t>, SkBitmap>,
                                std::string> {
            ASSIGN_OR_RETURN(SkBitmap redacted_bitmap,
                             PageContextFetcher::RedactScreenshotOnWorkerThread(
                                 bitmap, visible_bounding_boxes_for_redaction,
                                 redaction_color));

            std::optional<std::vector<uint8_t>> encoded =
                EncodeScreenshot(redacted_bitmap, screenshot_collection_options);
            base::expected<std::pair<std::vector<uint8_t>, SkBitmap>,
                           std::string>
                reply;
            if (encoded) {
              reply.emplace(
                  std::make_pair(std::move(encoded.value()), redacted_bitmap));
            } else {
              reply = base::unexpected("JPEGCodec failed to encode");
            }
            return reply;
          },
          *screenshot_bitmap_, std::move(visible_bounding_boxes_for_redaction),
          screenshot_redaction_color_, screenshot_collection_options_),
      base::BindOnce(&PageContextFetcher::ReceivedEncodedScreenshot,
                     GetWeakPtr()));
  screenshot_bitmap_.reset();
}

void PageContextFetcher::RedactAndEncodeScreenshotIfNeeded() {
  if (!screenshot_bitmap_) {
    return;
  }

  if (!screenshot_needs_redaction_using_apc_) {
    RedactAndEncodeScreenshot(
        std::move(tracked_element_bounds_for_screenshot_redaction_));
    return;
  }

  // We need APC to determine if redaction is necessary.
  if (!annotated_page_content_done_) {
    return;
  }

  std::vector<gfx::Rect> visible_bounding_boxes_for_redaction =
      std::move(tracked_element_bounds_for_screenshot_redaction_);

  // Once APC is done, any requested password, OTP, or sensitive-payment
  // redaction implies we have final bounding boxes to redact.
  CHECK(pending_result_);
  CHECK(pending_result_->annotated_page_content_result.has_value());

  double scale_x = 1.0;
  double scale_y = 1.0;
  if (!original_view_size_pixels_.IsEmpty() && !screenshot_bitmap_->empty()) {
    scale_x = static_cast<double>(screenshot_bitmap_->width()) /
              original_view_size_pixels_.width();
    scale_y = static_cast<double>(screenshot_bitmap_->height()) /
              original_view_size_pixels_.height();
  }

  const std::vector<gfx::Rect>& visible_bounding_boxes_for_redaction_from_apc =
      pending_result_->annotated_page_content_result
          ->visible_bounding_boxes_for_redaction;
  for (const gfx::Rect& rect : visible_bounding_boxes_for_redaction_from_apc) {
    visible_bounding_boxes_for_redaction.push_back(
        gfx::ScaleToEnclosingRect(rect, scale_x, scale_y));
  }

  RedactAndEncodeScreenshot(std::move(visible_bounding_boxes_for_redaction));
}

// content::WebContentsObserver impl.
void PageContextFetcher::PrimaryPageChanged(content::Page& page) {
  primary_page_changed_ = true;
  RunCallbackIfComplete();
}

void PageContextFetcher::OnScreenshotTimeout() {
  // When any redaction is enabled, the screenshot must wait for APC to finish
  // because APC is what produces the final bounding boxes we redact.
  //
  // The screenshot timer is intended to catch hangs during the initial bitmap
  // capture. If we have already received the bitmap, we should ignore this
  // timeout and allow the process to continue. This prevents missing
  // screenshots when the capture was successful but APC is slow. In such
  // cases, we rely on the APC-specific timeouts to eventually terminate the
  // request if it hangs.
  //
  // It is also acceptable not to timeout during the encoding phase because it
  // runs on a browser worker thread.
  if (screenshot_capture_done_) {
    return;
  }

  ReceivedEncodedScreenshot(base::unexpected("ScreenshotTimeout"));
}

void PageContextFetcher::ReceivedEncodedScreenshot(
    base::expected<std::pair<std::vector<uint8_t>, SkBitmap>, std::string>
        screenshot_data) {
  // This function can be called multiple times, for timeout behavior. Early
  // exit if it's already been called.
  if (screenshot_done_) {
    return;
  }
  auto elapsed = elapsed_timer_.Elapsed();
  screenshot_done_ = true;
  capture_count_lock_ = {};
  if (screenshot_data.has_value()) {
    pending_result_->screenshot_result.value().screenshot_data =
        std::move(screenshot_data.value().first);
    switch (GetScreenshotImageType(screenshot_collection_options_)) {
      case ScreenshotImageType::kJpeg:
        pending_result_->screenshot_result.value().mime_type = "image/jpeg";
        break;
      case ScreenshotImageType::kPng:
        pending_result_->screenshot_result.value().mime_type = "image/png";
        break;
      case ScreenshotImageType::kWebp:
        pending_result_->screenshot_result.value().mime_type = "image/webp";
        break;
      default:
        NOTREACHED();
    }
    base::UmaHistogramTimes("Glic.PageContextFetcher.GetEncodedScreenshot",
                            elapsed);
    if (progress_listener_) {
      progress_listener_->ScreenshotRedacted(screenshot_data.value().second);
      progress_listener_->EndScreenshot(std::nullopt);
    }
  } else {
    pending_result_->screenshot_result =
        base::unexpected(screenshot_data.error());
    base::UmaHistogramTimes(
        "Glic.PageContextFetcher.GetEncodedScreenshot.Failure", elapsed);
    if (progress_listener_) {
      progress_listener_->EndScreenshot(screenshot_data.error());
    }
  }
  if (pending_result_->screenshot_result.has_value()) {
    pending_result_->screenshot_result.value().end_time =
        base::TimeTicks::Now();
  }
  RunCallbackIfComplete();
}

void PageContextFetcher::ReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  // Get trimmed text without copying.
  std::string trimmed_text = std::move(result->inner_text);
  bool truncated =
      TruncateUTF8ToByteLimit(trimmed_text, inner_text_bytes_limit_);

  pending_result_->inner_text_result.emplace(
      std::move(trimmed_text), std::move(result->node_offset), truncated);
  inner_text_done_ = true;
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetInnerText",
                          elapsed_timer_.Elapsed());
  RunCallbackIfComplete();
}

void PageContextFetcher::ReceivedAnnotatedPageContent(
    optimization_guide::AIPageContentResultOrError content) {
  const bool has_result = content.has_value();
  if (has_result) {
    pending_result_->annotated_page_content_result.emplace(
        std::move(content.value()));
    screenshot_needs_redaction_using_apc_ =
        !pending_result_->annotated_page_content_result
             ->visible_bounding_boxes_for_redaction.empty();
  } else {
    pending_result_->annotated_page_content_result =
        base::unexpected(content.error());
    screenshot_needs_redaction_using_apc_ = false;
  }
  annotated_page_content_done_ = true;
  base::UmaHistogramTimes("Glic.PageContextFetcher.GetAnnotatedPageContent",
                          elapsed_timer_.Elapsed());
  if (progress_listener_) {
    if (has_result) {
      progress_listener_->EndAPC(std::nullopt);
    } else {
      progress_listener_->EndAPC(
          absl::StrFormat("Failed: %s", content.error()));
    }
  }

  MaybeAddIframeInfo();
  RedactAndEncodeScreenshotIfNeeded();

  RunCallbackIfComplete();
}

void PageContextFetcher::RunCallbackIfComplete() {
  // `callback_` may already be null if the fetch completed earlier.
  if (!initialization_done_ || !callback_) {
    return;
  }

  bool web_contents_went_away =
      !web_contents() || !web_contents()->GetPrimaryMainFrame();

  bool all_tasks_complete = (screenshot_done_ && inner_text_done_ &&
                             annotated_page_content_done_ && pdf_done_);
  bool page_still_valid = !primary_page_changed_ && !web_contents_went_away;
  if (!all_tasks_complete && page_still_valid) {
    return;
  }

  base::UmaHistogramTimes("Glic.PageContextFetcher.Total",
                          elapsed_timer_.Elapsed());

  if (web_contents_went_away) {
    std::move(callback_).Run(base::unexpected(FetchPageContextErrorDetails{
        FetchPageContextError::kWebContentsWentAway,
        "web contents went away"}));
    return;
  }

  if (primary_page_changed_) {
    std::move(callback_).Run(base::unexpected(FetchPageContextErrorDetails{
        FetchPageContextError::kWebContentsChanged, "web contents changed"}));
    return;
  }

  std::move(callback_).Run(base::ok(std::move(pending_result_)));
}

void PageContextFetcher::ProcessTrackedElementRects(
    const viz::TrackedElementRects& tracked_element_rects) {
  CollectTrackedElementRectsForIframes(tracked_element_rects);
  CollectTrackedElementRectsForPassword(tracked_element_rects);
}

void PageContextFetcher::CollectTrackedElementRectsForIframes(
    const viz::TrackedElementRects& tracked_element_rects) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kAIPageContentTrackedElementsIframe)) {
    return;
  }

  iframe_info_.clear();
  const auto iframe_tracking_feature =
      viz::TrackedElementFeature::kIframeTracking;
  if (!tracked_element_rects.contains(iframe_tracking_feature)) {
    return;
  }

  // Build a map from local frame token to RFH. We can use this to get the
  // RFH associated with a tracked element's parent_frame_token.
  base::flat_map<blink::LocalFrameToken, content::RenderFrameHost*>
      frame_token_to_rfh;
  if (web_contents()) {
    web_contents()->ForEachRenderFrameHost(
        [&frame_token_to_rfh](content::RenderFrameHost* rfh) {
          frame_token_to_rfh[rfh->GetFrameToken()] = rfh;
        });
  }

  // For each tracked iframe element, we get the parent RFH from the map above
  // and use it to get the renderer process id. Then we can get the iframe
  // RFH using the element's frame_token and the parent renderer process id.
  // We then use the iframe RFH to get the URL and origin.
  for (const viz::TrackedElementRect& element :
       tracked_element_rects.at(iframe_tracking_feature)) {
    optimization_guide::proto::IframeInfo iframe_info;
    const gfx::Rect& bounds = element.visible_bounds;
    iframe_info.mutable_bounding_box()->set_x(bounds.origin().x());
    iframe_info.mutable_bounding_box()->set_y(bounds.origin().y());
    iframe_info.mutable_bounding_box()->set_width(bounds.width());
    iframe_info.mutable_bounding_box()->set_height(bounds.height());
    iframe_info.mutable_bounding_box()->set_is_screenshot_relative(true);

    // Iframe tracked elements should always have a parent frame token and a
    // frame token.
    if (element.frame_token.has_value() &&
        element.parent_frame_token.has_value()) {
      // If we can't find the RFH associated with the iframe, we cannot
      // determine the iframe's url/origin. This could happen if the screenshot
      // is displaying stale content. We should leave the url and origin empty
      // in this case.
      auto it = frame_token_to_rfh.find(element.parent_frame_token.value());
      if (it != frame_token_to_rfh.end()) {
        content::RenderFrameHost* parent_rfh = it->second;
        int renderer_process_id = parent_rfh->GetProcess()->GetID().value();
        content::RenderFrameHost* iframe_rfh =
            optimization_guide::GetRenderFrameHostForToken(
                renderer_process_id, element.frame_token.value());
        if (iframe_rfh) {
          iframe_info.set_url(iframe_rfh->GetLastCommittedURL().spec());
          optimization_guide::SecurityOriginSerializer::Serialize(
              iframe_rfh->GetLastCommittedOrigin(),
              iframe_info.mutable_security_origin());
        }
      }
    }
    iframe_info_.push_back(std::move(iframe_info));
  }
}

void PageContextFetcher::MaybeAddIframeInfo() {
  // We need to wait for:
  // 1. Screenshot capture to complete, because iframe layout metadata is
  //    extracted from the tracked element rects obtained during screenshot.
  // 2. Annotated Page Content (APC) extraction to complete (if requested).
  //    This is because we need to copy `screenshot_info` to the nested, legacy
  //    APC field for backward compatibility.
  if (!screenshot_capture_done_ || !annotated_page_content_done_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kAIPageContentTrackedElementsIframe)) {
    return;
  }

  if (iframe_info_.empty()) {
    base::UmaHistogramBoolean("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                              false);
    return;
  }

  // Populate the standalone screenshot_info on FetchPageContextResult.
  pending_result_->screenshot_info.emplace();
  if (pending_result_->screenshot_result.has_value()) {
    pending_result_->screenshot_info->mutable_screenshot_size()->set_width(
        pending_result_->screenshot_result->dimensions.width());
    pending_result_->screenshot_info->mutable_screenshot_size()->set_height(
        pending_result_->screenshot_result->dimensions.height());
  }
  size_t iframe_proto_size = 0;
  for (const auto& iframe_info : iframe_info_) {
    base::UmaHistogramBoolean("Glic.PageContextFetcher.IframeInfoHasUrlOrigin",
                              iframe_info.has_security_origin());
    *pending_result_->screenshot_info->add_iframe_info() = iframe_info;
    iframe_proto_size += iframe_info.ByteSizeLong();
  }

  base::UmaHistogramCounts10000(
      "Glic.PageContextFetcher.ScreenshotInfo.IframeInfo.ProtoSize",
      base::saturated_cast<int>(iframe_proto_size));

  // Also copy to the field inside AnnotatedPageContent to ensure backward
  // compatibility.
  if (pending_result_->annotated_page_content_result.has_value()) {
    *pending_result_->annotated_page_content_result->proto
         .mutable_gemini_in_chrome_page_metadata()
         ->mutable_screenshot_info() = *pending_result_->screenshot_info;
  }

  base::UmaHistogramBoolean("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                            true);
}

void PageContextFetcher::CollectTrackedElementRectsForPassword(
    const viz::TrackedElementRects& tracked_element_rects) {
  if (!(base::FeatureList::IsEnabled(kGlicScreenshotPasswordRedaction) &&
        base::FeatureList::IsEnabled(
            blink::features::kAIPageContentTrackedElementsPassword))) {
    return;
  }

  auto it =
      tracked_element_rects.find(viz::TrackedElementFeature::kPasswordTracking);
  if (it == tracked_element_rects.end()) {
    return;
  }
  for (const viz::TrackedElementRect& rect : it->second) {
    tracked_element_bounds_for_screenshot_redaction_.push_back(
        rect.visible_bounds);
  }
}

base::WeakPtr<PageContextFetcher> PageContextFetcher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string ToString(FetchPageContextError error) {
  switch (error) {
    case FetchPageContextError::kUnknown:
      return "kUnknown";
    case FetchPageContextError::kWebContentsChanged:
      return "kWebContentsChanged";
    case FetchPageContextError::kPageContextNotEligible:
      return "kPageContextNotEligible";
    case FetchPageContextError::kWebContentsWentAway:
      return "kWebContentsWentAway";
  }
}

BASE_FEATURE(kGlicTabScreenshotExperiment, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicScreenshotPasswordRedaction,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGlicScreenshotSensitivePaymentRedaction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicEmbeddedPdfBytesExtraction,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageContextFetcherPdfExtraction,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPdfExtractionTimeout{
    &kPageContextFetcherPdfExtraction, "timeout", base::Seconds(5)};

const base::FeatureParam<int> kMaxScreenshotWidthParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_width", 0};

const base::FeatureParam<int> kMaxScreenshotHeightParam{
    &kGlicTabScreenshotExperiment, "max_screenshot_height", 0};

const base::FeatureParam<int> kScreenshotQuality{&kGlicTabScreenshotExperiment,
                                                 "screenshot_quality", 40};

const base::FeatureParam<std::string> kScreenshotImageType{
    &kGlicTabScreenshotExperiment, "screenshot_image_type", "jpeg"};

const base::FeatureParam<base::TimeDelta> kScreenshotTimeout{
    &kGlicTabScreenshotExperiment, "screenshot_timeout_ms", base::Seconds(5)};

const base::FeatureParam<base::TimeDelta> kScreenshotTimeoutBrowserAllowance{
    &kGlicTabScreenshotExperiment, "screenshot_timeout_allowance_ms",
    base::Milliseconds(500)};

FetchPageContextResult::FetchPageContextResult()
    : screenshot_result(base::unexpected("Uninitialized")),
      annotated_page_content_result(base::unexpected("Uninitialized")) {}

FetchPageContextResult::FetchPageContextResult(FetchPageContextResult&&) =
    default;
FetchPageContextResult& FetchPageContextResult::operator=(
    FetchPageContextResult&&) = default;

FetchPageContextResult::~FetchPageContextResult() = default;

PdfResult::PdfResult(url::Origin origin, std::vector<uint8_t> bytes)
    : origin(std::move(origin)), data(std::move(bytes)) {}

PdfResult::PdfResult(url::Origin origin, std::string text)
    : origin(std::move(origin)), data(std::move(text)) {}

PdfResult::PdfResult(url::Origin origin)
    : origin(std::move(origin)), size_exceeded(true) {}

PdfResult::PdfResult(PdfResult&&) = default;
PdfResult& PdfResult::operator=(PdfResult&&) = default;

PdfResult::~PdfResult() = default;

ScreenshotResult::ScreenshotResult(gfx::Size dimensions)
    : dimensions(std::move(dimensions)) {}

ScreenshotResult::ScreenshotResult(ScreenshotResult&&) = default;
ScreenshotResult& ScreenshotResult::operator=(ScreenshotResult&&) = default;

ScreenshotResult::~ScreenshotResult() = default;

InnerTextResultWithTruncation::InnerTextResultWithTruncation(
    std::string inner_text,
    std::optional<unsigned> node_offset,
    bool truncated)
    : InnerTextResult(std::move(inner_text), node_offset),
      truncated(truncated) {}

InnerTextResultWithTruncation::~InnerTextResultWithTruncation() = default;

PageContentResultWithEndTime::PageContentResultWithEndTime(
    optimization_guide::AIPageContentResult&& result)
    : optimization_guide::AIPageContentResult(std::move(result)),
      end_time(base::TimeTicks::Now()) {}

}  // namespace page_content_annotations
