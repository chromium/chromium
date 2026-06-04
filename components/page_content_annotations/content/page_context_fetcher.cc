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
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
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
#include "components/pdf/common/constants.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/schemeful_site.h"
#include "pdf/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
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

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace page_content_annotations {

namespace {

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
  return new_size.width() / original_size.width();
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

// Combination of tracked states for when a PDF contents request is made by
// Glic. Must be kept in sync with PdfRequestStates in
// src/tools/metrics/histograms/metadata/glic/enums.xml.
enum class PdfRequestStates {
  kPdfMainDoc_PdfFound = 0,
  kPdfMainDoc_PdfNotFound = 1,
  kNonPdfMainDoc_PdfFound = 2,
  kNonPdfMainDoc_PdfNotFound = 3,
  kMaxValue = kNonPdfMainDoc_PdfNotFound,
};

// PDF support is controlled by the buildflag, not just by platform.
#if BUILDFLAG(ENABLE_PDF)
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
#endif  // BUILDFLAG(ENABLE_PDF)

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
    std::unique_ptr<FetchPageProgressListener> progress_listener)
    : get_screenshot_service_callback_(
          std::move(get_screenshot_service_callback)),
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
    GetInnerText(
        *frame,
        /*node_id=*/std::nullopt,
        base::BindOnce(&PageContextFetcher::ReceivedInnerText, GetWeakPtr()));
  } else {
    inner_text_done_ = true;
  }

  pdf_done_ = true;  // Will not fetch PDF contents by default.
#if BUILDFLAG(ENABLE_PDF)
  if (options.pdf_options && options.pdf_options->size_limit() > 0) {
    FetchPdfContent(*(options.pdf_options));
  }
#endif  // BUILDFLAG(ENABLE_PDF)

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

// TODO: Enable pdf fetching for Android.
// PDF support is compiled out on some platforms, including Fuchsia.
#if BUILDFLAG(ENABLE_PDF)
void PageContextFetcher::FetchPdfContent(const PdfOptions& options) {
  bool is_pdf_document =
      web_contents()->GetContentsMimeType() == pdf::kPDFMimeType;
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());

  if (options.format() == PdfOptions::Format::kBytes) {
    // This metric is specific to Glic, which requests PDF bytes only.
    RecordPdfRequestState(is_pdf_document,
                          /*pdf_found=*/pdf_helper != nullptr);
  }

  // When document load is not complete:
  // - GetPdfBytes() is not safe.
  // - GetPageText() is safe but returns an empty string.
  //
  // PageContextFetcher is only responsible for fetching page context. It is not
  // responsible for waiting for the page (including PDF) being stable --
  // clients should ensure page stability and manage the timing of extraction.
  // Clients should not rely on the `IsDocumentLoadComplete()` check below.
  //
  // See comments in `AnnotatedPageContentRequest::RequestPdfText` for more
  // information about the timing of PDF text extraction.
  if (is_pdf_document && pdf_helper && pdf_helper->IsDocumentLoadComplete()) {
    switch (options.format()) {
      case PdfOptions::Format::kBytes: {
        pdf_helper->GetPdfBytes(
            options.size_limit(),
            base::BindOnce(
                &PageContextFetcher::ReceivedPdfBytes, GetWeakPtr(),
                pdf_helper->render_frame_host().GetLastCommittedOrigin(),
                options.size_limit()));
        pdf_done_ = false;  // Will fetch PDF bytes.
        break;
      }
      case PdfOptions::Format::kText: {
        // The PDF text is restricted to first page only. This is intentional
        // for performance considerations. Extracting from a rendered page is
        // more efficient than extracting from an unrendered page.
        //
        // TODO(b/506129567): The size limit is currently enforced after the
        // text is retrieved from `PDFDocumentHelper::GetPageText`. It can be
        // more efficient if enforced within this method, inside the PDFium.
        pdf_helper->GetPageText(
            /*page_index=*/0,
            base::BindOnce(
                &PageContextFetcher::ReceivedPdfText, GetWeakPtr(),
                pdf_helper->render_frame_host().GetLastCommittedOrigin(),
                options.size_limit()));
        pdf_done_ = false;  // Will fetch PDF first page text.
        break;
      }
    }
  } else if (options.format() == PdfOptions::Format::kText) {
    // The PDF text extraction is requested but not executed, record failure
    // status.
    if (!is_pdf_document) {
      RecordPdfTextExtractionStatus(PdfTextExtractionStatus::kNotPdf);
    } else if (!pdf_helper) {
      RecordPdfTextExtractionStatus(
          PdfTextExtractionStatus::kPdfExtractionNotAvailable);
    } else if (!pdf_helper->IsDocumentLoadComplete()) {
      RecordPdfTextExtractionStatus(
          PdfTextExtractionStatus::kPdfDocumentNotLoaded);
    }
  }
}

void PageContextFetcher::ReceivedPdfBytes(
    url::Origin pdf_origin,
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
    pending_result_->pdf_result.emplace(std::move(pdf_origin));
  } else {
    pending_result_->pdf_result.emplace(std::move(pdf_origin), pdf_bytes);
  }
  RunCallbackIfComplete();
}

void PageContextFetcher::ReceivedPdfText(url::Origin pdf_origin,
                                         uint32_t text_byte_limit,
                                         const std::u16string& text) {
  pdf_done_ = true;

  base::UmaHistogramTimes(kPdfTextExtractionLatencyHistogram,
                          elapsed_timer_.Elapsed());

  if (text.empty()) {
    // Note an empty text does not necessarily imply there is something wrong
    // with the extraction. It is possible that the PDF is blank.
    RecordPdfTextExtractionStatus(PdfTextExtractionStatus::kEmptyText);
  } else {
    RecordPdfTextExtractionStatus(PdfTextExtractionStatus::kSuccess);
  }

  // Create a UTF-16 string view that contains at most `text_byte_limit` number
  // of chars. There is no need to convert the UTF-16 chars beyond this view
  // since they cannot be within the byte limit, as one char occupies at least
  // one bytes.
  std::u16string_view text_view(text);
  if (text_view.size() > text_byte_limit) {
    text_view = text_view.substr(0, text_byte_limit);
  }

  // Convert to UTF-8 string.
  std::string utf8_text = base::UTF16ToUTF8(text_view);
  base::UmaHistogramCounts1M(kPdfTextExtractionSizeHistogram, utf8_text.size());

  // Truncate the `utf8_text` to the `text_byte_limit`.
  bool size_exceeded = TruncateUTF8ToByteLimit(utf8_text, text_byte_limit);

  // Move construct the PDF result.
  pending_result_->pdf_result.emplace(std::move(pdf_origin),
                                      std::move(utf8_text));
  pending_result_->pdf_result->size_exceeded = size_exceeded;

  RunCallbackIfComplete();
}
#endif  // BUILDFLAG(ENABLE_PDF)

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
        .scale_factor = GetScreenshotScaleFactor(
            view_size,
            GetScreenshotSize(view_size, screenshot_collection_options_)),
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

    view->CopyFromSurface(
        gfx::Rect(),  // Copy entire surface area.
        GetScreenshotSize(view_size, screenshot_collection_options_),
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
    MaybeAddIframeInfoToAPC();
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

  const std::vector<gfx::Rect>& visible_bounding_boxes_for_redaction_from_apc =
      pending_result_->annotated_page_content_result
          ->visible_bounding_boxes_for_redaction;
  visible_bounding_boxes_for_redaction.insert(
      visible_bounding_boxes_for_redaction.end(),
      visible_bounding_boxes_for_redaction_from_apc.begin(),
      visible_bounding_boxes_for_redaction_from_apc.end());

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

  MaybeAddIframeInfoToAPC();
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

void PageContextFetcher::MaybeAddIframeInfoToAPC() {
  // We need to wait for both the screenshot capture to be done and the APC to
  // be done before adding the iframe info to the APC.
  if (!screenshot_capture_done_ || !annotated_page_content_done_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kAIPageContentTrackedElementsIframe)) {
    return;
  }

  // If we don't have an APC result or iframe info, there's nothing to do.
  // TODO(b/441532128): If we don't have an APC result, we should create one and
  // populate the screenshot iframe info.
  if (!pending_result_->annotated_page_content_result.has_value() ||
      iframe_info_.empty()) {
    base::UmaHistogramBoolean("Glic.PageContextFetcher.IframeInfoAddedToAPC",
                              false);
    return;
  }

  // Add the iframe bounding boxes and url/origin data to the screenshot info.
  optimization_guide::proto::ScreenshotInfo* screenshot_info =
      pending_result_->annotated_page_content_result->proto
          .mutable_gemini_in_chrome_page_metadata()
          ->mutable_screenshot_info();
  for (const auto& iframe_info : iframe_info_) {
    base::UmaHistogramBoolean("Glic.PageContextFetcher.IframeInfoHasUrlOrigin",
                              iframe_info.has_security_origin());
    *screenshot_info->add_iframe_info() = iframe_info;
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
