// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/page_content_annotations/content/page_content_screenshot_service.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/redaction_params.h"
#include "components/pdf/common/constants.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/pdf/browser/pdf_document_helper.h"
#endif

namespace page_content_annotations {

namespace {

gfx::Size GetScreenshotSize(const gfx::Size& original_size) {
  // By default, no scaling.
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return gfx::Size();
  }

  // If either width or height is 0, or the view is empty, no scaling.
  int max_width = kMaxScreenshotWidthParam.Get();
  int max_height = kMaxScreenshotHeightParam.Get();
  if (max_width == 0 || max_height == 0 || original_size.IsEmpty()) {
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
  return new_size.width() / original_size.width();
}

int GetScreenshotJpegQuality() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return 40;
  }
  // Must be an int from 0 to 100.
  return std::max(0, std::min(100, kScreenshotQuality.Get()));
}

int GetScreenshotWebPQuality() {
  return GetScreenshotJpegQuality();
}

// Png only has two modes exposed, so we use the quality to determine if it is
// low quality or not by checking if it is 50 or lower.
bool ShouldPngScreenshotBeLowQuality() {
  if (!base::FeatureList::IsEnabled(kGlicTabScreenshotExperiment)) {
    return false;
  }
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
constexpr base::TimeDelta kScreenshotTimeoutBrowserAllowance =
    base::Milliseconds(500);

ScreenshotImageType GetScreenshotImageType() {
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

SkBitmap RedactScreenshotOnWorkerThread(
    const SkBitmap& bitmap,
    std::vector<gfx::Rect> visible_bounding_boxes_for_redaction,
    SkColor4f redaction_color) {
  if (visible_bounding_boxes_for_redaction.empty()) {
    return bitmap;
  }

  SkBitmap redacted_bitmap;
  redacted_bitmap.setInfo(bitmap.info());
  redacted_bitmap.allocPixels();

  SkCanvas canvas(redacted_bitmap);
  SkPaint color;
  color.setColor(redaction_color);
  for (const auto& rect : visible_bounding_boxes_for_redaction) {
    canvas.drawRect(RectToSkRect(rect), color);
  }

  return redacted_bitmap;
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

// Combination of tracked states for when a PDF contents request is made.
// Must be kept in sync with PdfRequestStates in
// src/tools/metrics/histograms/metadata/glic/enums.xml.
enum class PdfRequestStates {
  kPdfMainDoc_PdfFound = 0,
  kPdfMainDoc_PdfNotFound = 1,
  kNonPdfMainDoc_PdfFound = 2,
  kNonPdfMainDoc_PdfNotFound = 3,
  kMaxValue = kNonPdfMainDoc_PdfNotFound,
};

#if !BUILDFLAG(IS_ANDROID)
void RecordPdfRequestState(bool is_pdf_document, bool pdf_found) {
  PdfRequestStates state;
  if (is_pdf_document) {
    state = pdf_found ? PdfRequestStates::kPdfMainDoc_PdfFound
                      : PdfRequestStates::kPdfMainDoc_PdfNotFound;
  } else {
    state = pdf_found ? PdfRequestStates::kNonPdfMainDoc_PdfFound
                      : PdfRequestStates::kNonPdfMainDoc_PdfNotFound;
  }
  UMA_HISTOGRAM_ENUMERATION("Glic.TabContext.PdfContentsRequested", state);
}
#endif

}  // namespace

PageContextFetcher::PageContextFetcher(
    GetScreenshotServiceCallback get_screenshot_service_callback,
    std::unique_ptr<FetchPageProgressListener> progress_listener)
    : get_screenshot_service_callback_(
          std::move(get_screenshot_service_callback)),
      progress_listener_(std::move(progress_listener)) {}
PageContextFetcher::~PageContextFetcher() = default;

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
    GetInnerText(
        *frame,
        /*node_id=*/std::nullopt,
        base::BindOnce(&PageContextFetcher::ReceivedInnerText, GetWeakPtr()));
  } else {
    inner_text_done_ = true;
  }

  pdf_done_ = true;  // Will not fetch PDF contents by default.
#if !BUILDFLAG(IS_ANDROID)
  if (options.pdf_size_limit > 0) {
    bool is_pdf_document =
        web_contents()->GetContentsMimeType() == pdf::kPDFMimeType;
    pdf::PDFDocumentHelper* pdf_helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
    RecordPdfRequestState(is_pdf_document,
                          /*pdf_found=*/pdf_helper != nullptr);
    // GetPdfBytes() is not safe before IsDocumentLoadComplete() = true.
    if (is_pdf_document && pdf_helper && pdf_helper->IsDocumentLoadComplete()) {
      const url::Origin& pdf_origin =
          pdf_helper->render_frame_host().GetLastCommittedOrigin();
      pdf_helper->GetPdfBytes(
          options.pdf_size_limit,
          base::BindOnce(&PageContextFetcher::ReceivedPdfBytes, GetWeakPtr(),
                         pdf_origin, options.pdf_size_limit));
      pdf_done_ = false;  // Will fetch PDF contents.
    }
  }
#endif

  if (options.annotated_page_content_options) {
    blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
        options.annotated_page_content_options.Clone();
    ai_page_content_options->on_critical_path = true;
    if (progress_listener_) {
      progress_listener_->BeginAPC();
    }
    ai_page_content_options->include_passwords_for_redaction =
        base::FeatureList::IsEnabled(kGlicScreenshotPasswordRedaction);
    screenshot_needs_password_redaction_ =
        ai_page_content_options->include_passwords_for_redaction;
    ai_page_content_options->include_sensitive_payments_for_redaction =
        base::FeatureList::IsEnabled(kGlicScreenshotSensitivePaymentRedaction);
    screenshot_needs_sensitive_payment_redaction_ =
        ai_page_content_options->include_sensitive_payments_for_redaction;
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

// TODO: Enable pdf fetching for Android.
#if !BUILDFLAG(IS_ANDROID)
void PageContextFetcher::ReceivedPdfBytes(
    const url::Origin& pdf_origin,
    uint32_t pdf_size_limit,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& pdf_bytes,
    uint32_t page_count) {
  pdf_done_ = true;

  // Warning!: `pdf_bytes_` can be larger than pdf_size_limit.
  // `pdf_size_limit` applies to the original PDF size, but the PDF is
  // re-serialized and returned, so it is not identical to the original.
  bool size_limit_exceeded =
      status == pdf::mojom::PdfListener_GetPdfBytesStatus::kSizeLimitExceeded ||
      pdf_bytes.size() > pdf_size_limit;

  if (size_limit_exceeded) {
    pending_result_->pdf_result.emplace(pdf_origin);
  } else {
    pending_result_->pdf_result.emplace(pdf_origin, pdf_bytes);
  }
  RunCallbackIfComplete();
}
#endif

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

  gfx::Size view_size = view->GetViewBounds().size();

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
    }
    PageContentScreenshotService::RequestParams request_params = {
        .clip_rect = clip_rect,
        .scale_factor =
            GetScreenshotScaleFactor(view_size, GetScreenshotSize(view_size)),
        .clip_x_coord_override = clip_coord_override,
        .clip_y_coord_override = clip_coord_override,
        .redaction_params = std::move(redaction_params),
        .max_per_capture_bytes =
            screenshot_options.paint_preview_options()->max_per_capture_bytes,
    };
    service->RequestScreenshot(
        &web_contents, std::move(request_params),
        base::BindOnce(&PageContextFetcher::ReceivedViewportBitmapOrError,
                       GetWeakPtr()));
  } else {
    SetCaptureCountLock(web_contents);
    ScheduleScreenshotTimeout();

    view->CopyFromSurface(
        gfx::Rect(),  // Copy entire surface area.
        GetScreenshotSize(view_size), kScreenshotTimeout.Get(),
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
      kScreenshotTimeout.Get() + kScreenshotTimeoutBrowserAllowance);
}

void PageContextFetcher::ReceivedViewportBitmap(
    const content::CopyFromSurfaceResult& result) {
  if (!result.has_value()) {
    base::UmaHistogramEnumeration("Glic.PageContextFetcher.GetScreenshotError",
                                  result.error());
    ReceivedViewportBitmapOrError(
        base::unexpected<std::string>(ToString(result.error())));
    return;
  }

  ReceivedViewportBitmapOrError(base::ok(&result->bitmap));
}

void PageContextFetcher::ReceivedViewportBitmapOrError(
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
             SkColor4f redaction_color) {
            SkBitmap redacted_bitmap = RedactScreenshotOnWorkerThread(
                bitmap, visible_bounding_boxes_for_redaction, redaction_color);
            std::optional<std::vector<uint8_t>> encoded;
            switch (GetScreenshotImageType()) {
              case ScreenshotImageType::kJpeg:
                encoded = gfx::JPEGCodec::Encode(redacted_bitmap,
                                                 GetScreenshotJpegQuality());
                break;
              case ScreenshotImageType::kPng:
                if (ShouldPngScreenshotBeLowQuality()) {
                  encoded = gfx::PNGCodec::FastEncodeBGRASkBitmap(
                      redacted_bitmap, /*discard_transparency=*/true);
                } else {
                  encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
                      redacted_bitmap, /*discard_transparency=*/true);
                }
                break;
              case ScreenshotImageType::kWebp:
                encoded = gfx::WebpCodec::Encode(redacted_bitmap,
                                                 GetScreenshotWebPQuality());
                break;
              default:
                break;
            }
            base::expected<std::vector<uint8_t>, std::string> reply;
            if (encoded) {
              reply.emplace(std::move(encoded.value()));
            } else {
              reply = base::unexpected("JPEGCodec failed to encode");
            }
            return reply;
          },
          *screenshot_bitmap_, std::move(visible_bounding_boxes_for_redaction),
          screenshot_redaction_color_),
      base::BindOnce(&PageContextFetcher::ReceivedEncodedScreenshot,
                     GetWeakPtr()));
  screenshot_bitmap_.reset();
}

void PageContextFetcher::RedactAndEncodeScreenshotIfNeeded() {
  if (!screenshot_bitmap_) {
    return;
  }

  if (!screenshot_needs_password_redaction_ &&
      !screenshot_needs_sensitive_payment_redaction_) {
    RedactAndEncodeScreenshot({});
    return;
  }

  // We need APC to determine if redaction is necessary.
  if (!annotated_page_content_done_) {
    return;
  }

  // If APC extraction is done and we've determined password/sensitive payment
  // redaction is needed, it implies we have a result with bounding boxes to
  // redact.
  CHECK(pending_result_);
  CHECK(pending_result_->annotated_page_content_result.has_value());

  std::vector<gfx::Rect> visible_bounding_boxes_for_redaction =
      pending_result_->annotated_page_content_result
          ->visible_bounding_boxes_for_password_redaction;
  visible_bounding_boxes_for_redaction.insert(
      visible_bounding_boxes_for_redaction.end(),
      pending_result_->annotated_page_content_result
          ->visible_bounding_boxes_for_sensitive_payment_redaction.begin(),
      pending_result_->annotated_page_content_result
          ->visible_bounding_boxes_for_sensitive_payment_redaction.end());
  RedactAndEncodeScreenshot(std::move(visible_bounding_boxes_for_redaction));
}

// content::WebContentsObserver impl.
void PageContextFetcher::PrimaryPageChanged(content::Page& page) {
  primary_page_changed_ = true;
  RunCallbackIfComplete();
}

void PageContextFetcher::OnScreenshotTimeout() {
  // When password/sensitive payment redaction is enabled, the screenshot must
  // wait for APC to finish before it can be encoded.
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
    base::expected<std::vector<uint8_t>, std::string> screenshot_data) {
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
        std::move(screenshot_data.value());
    switch (GetScreenshotImageType()) {
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
  size_t truncated_size =
      base::TruncateUTF8ToByteSize(trimmed_text, inner_text_bytes_limit_)
          .length();
  bool truncated = false;
  if (truncated_size < trimmed_text.length()) {
    truncated = true;
    trimmed_text.resize(truncated_size);
  }

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
    screenshot_needs_password_redaction_ =
        !pending_result_->annotated_page_content_result
             ->visible_bounding_boxes_for_password_redaction.empty();
    screenshot_needs_sensitive_payment_redaction_ =
        !pending_result_->annotated_page_content_result
             ->visible_bounding_boxes_for_sensitive_payment_redaction.empty();
  } else {
    pending_result_->annotated_page_content_result =
        base::unexpected(content.error());
    screenshot_needs_password_redaction_ = false;
    screenshot_needs_sensitive_payment_redaction_ = false;
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

  RedactAndEncodeScreenshotIfNeeded();

  RunCallbackIfComplete();
}

void PageContextFetcher::RunCallbackIfComplete() {
  if (!initialization_done_) {
    return;
  }

  // Continue only if the primary page changed or work is complete.
  bool work_complete = (screenshot_done_ && inner_text_done_ &&
                        annotated_page_content_done_ && pdf_done_) ||
                       primary_page_changed_;
  if (!work_complete) {
    return;
  }
  base::UmaHistogramTimes("Glic.PageContextFetcher.Total",
                          elapsed_timer_.Elapsed());

  if (!web_contents() || !web_contents()->GetPrimaryMainFrame()) {
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

FetchPageContextOptions::FetchPageContextOptions() = default;

FetchPageContextOptions::~FetchPageContextOptions() = default;

FetchPageContextResult::FetchPageContextResult()
    : screenshot_result(base::unexpected("Uninitialized")),
      annotated_page_content_result(base::unexpected("Uninitialized")) {}

FetchPageContextResult::~FetchPageContextResult() = default;

PdfResult::PdfResult(url::Origin origin, std::vector<uint8_t> bytes)
    : origin(std::move(origin)), bytes(std::move(bytes)) {}

PdfResult::PdfResult(url::Origin origin)
    : origin(std::move(origin)), size_exceeded(true) {}

PdfResult::~PdfResult() = default;

ScreenshotResult::ScreenshotResult(gfx::Size dimensions)
    : dimensions(std::move(dimensions)) {}

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
