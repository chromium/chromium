// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/pdf_content_fetcher.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "pdf/mojom/pdf.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace page_content_annotations {

namespace {

// Holds the result callback shared by the reply and the drop handler of the PDF
// extraction IPC, only one of which runs.
struct PdfExtractionState : public base::RefCounted<PdfExtractionState> {
  explicit PdfExtractionState(FetchPdfContentResultCallback cb)
      : callback(std::move(cb)) {}
  FetchPdfContentResultCallback callback;

 private:
  friend class base::RefCounted<PdfExtractionState>;
  ~PdfExtractionState() = default;
};

// Drop handler for the PDF extraction Mojo callback. Invoked when the IPC pipe
// to PDFium, which is the renderer process responsible for the extraction,
// disconnects. For example, the iframe hosting the embedded PDF navigates in
// the middle of the extraction. There will no longer be an extraction result.
void OnPdfPipeDisconnected(scoped_refptr<PdfExtractionState> state) {
  if (!state->callback || state->callback.IsCancelled()) {
    // The PDF extraction might have already timed out, there is no need to post
    // the task below.
    return;
  }

  // Handle the abort asynchronously so that, if the WebContents is being torn
  // down, its destruction completes before the posted task runs. The page
  // context fetch should resolve with the error
  // `FetchPageContextError::kWebContentsWentAway`, instead of returning a
  // partial result.
  //
  // PDF extraction is aborted. Continue extracting page context and treat PDF
  // extraction as complete with a null result.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(state->callback), std::nullopt));
}

void RecordPdfRequestState(bool is_top_level_pdf, bool pdf_found) {
  PdfRequestStates state;
  if (is_top_level_pdf) {
    state = pdf_found ? PdfRequestStates::kPdfMainDoc_PdfFound
                      : PdfRequestStates::kPdfMainDoc_PdfNotFound;
  } else {
    state = pdf_found ? PdfRequestStates::kNonPdfMainDoc_PdfFound
                      : PdfRequestStates::kNonPdfMainDoc_PdfNotFound;
  }
  base::UmaHistogramEnumeration(kPdfContentsRequestedHistogram, state);
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

// Finds the `PDFDocumentHelper` of the PDF extraction candidate in
// `WebContents`.
pdf::PDFDocumentHelper* GetPDFExtractionCandidate(
    content::WebContents& contents) {
  content::RenderFrameHost* primary_main_frame = contents.GetPrimaryMainFrame();
  if (!primary_main_frame) {
    return nullptr;
  }

  // Note: `root_view` can be nullptr while the `primary_main_frame` is still
  // valid. For example, while the user is dragging a tab between browser
  // windows. In that case, a default constructed `gfx::Rect` is used, which
  // intersects none of the PDF frames, which essentially falls back to the
  // behavior of `PDFDocumentHelper::MaybeGetForWebContents`.
  content::RenderWidgetHostView* root_view = contents.GetRenderWidgetHostView();
  const gfx::Rect root_view_bounds =
      root_view ? root_view->GetViewBounds() : gfx::Rect();

  pdf::PDFDocumentHelper* first_found = nullptr;
  pdf::PDFDocumentHelper* first_in_viewport = nullptr;

  primary_main_frame->ForEachRenderFrameHostWithAction(
      [&](content::RenderFrameHost* rfh) {
        auto* helper = pdf::PDFDocumentHelper::GetForCurrentDocument(rfh);
        if (!helper) {
          // This RenderFrameHost does not host a PDF, continue.
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        }

        if (!first_found) {
          // Found a PDF. Store it as a potential extraction candidate.
          first_found = helper;
        }

        content::RenderWidgetHostView* view = rfh->GetView();
        if (view && root_view_bounds.Intersects(view->GetViewBounds())) {
          // This PDF is within the viewport area. Stop searching.
          first_in_viewport = helper;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }

        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  // Prefer the in-viewport extraction candidate. If there is no in-viewport
  // candidate, return the first one found.
  return first_in_viewport ? first_in_viewport : first_found;
}

// TODO(b/562581431): This function should handle the
// `GetPdfBytesStatus::kFailed` extraction status.
void OnReceivedPdfBytes(scoped_refptr<PdfExtractionState> state,
                        url::Origin pdf_origin,
                        bool is_top_level_pdf,
                        uint32_t pdf_size_limit,
                        base::ElapsedTimer elapsed_timer,
                        pdf::mojom::PdfListener::GetPdfBytesStatus status,
                        const std::vector<uint8_t>& pdf_bytes,
                        uint32_t page_count) {
  // This function can be called after PDF extraction has reached timeout.
  // Early return in that case.
  if (!state->callback || state->callback.IsCancelled()) {
    return;
  }
  FetchPdfContentResultCallback callback = std::move(state->callback);

  // The sample is recorded in milliseconds.
  base::UmaHistogramTimes(is_top_level_pdf ? kPdfBytesTopLevelLatencyHistogram
                                           : kPdfBytesEmbeddedLatencyHistogram,
                          elapsed_timer.Elapsed());

  // The sample is recorded in KB. Bytes extraction size is capped at 64MB by
  // default.
  if (status == pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess) {
    base::UmaHistogramCounts100000(is_top_level_pdf
                                       ? kPdfBytesTopLevelSizeHistogram
                                       : kPdfBytesEmbeddedSizeHistogram,
                                   pdf_bytes.size() / 1024);
  }

  // Warning!: `pdf_bytes_` can be larger than pdf_size_limit.
  // `pdf_size_limit` applies to the original PDF size, but the PDF is
  // re-serialized and returned, so it is not identical to the original.
  bool size_limit_exceeded =
      status ==
          pdf::mojom::PdfListener::GetPdfBytesStatus::kSizeLimitExceeded ||
      pdf_bytes.size() > pdf_size_limit;

  base::UmaHistogramBoolean(is_top_level_pdf
                                ? kPdfBytesTopLevelSizeLimitExceededHistogram
                                : kPdfBytesEmbeddedSizeLimitExceededHistogram,
                            size_limit_exceeded);

  if (size_limit_exceeded) {
    std::move(callback).Run(PdfResult(std::move(pdf_origin)));
  } else {
    std::move(callback).Run(PdfResult(std::move(pdf_origin), pdf_bytes));
  }
}

void OnReceivedPdfText(scoped_refptr<PdfExtractionState> state,
                       url::Origin pdf_origin,
                       uint32_t text_byte_limit,
                       base::ElapsedTimer elapsed_timer,
                       const std::u16string& text) {
  // This function can be called after PDF extraction has reached timeout.
  // Early return in that case.
  if (!state->callback || state->callback.IsCancelled()) {
    return;
  }
  FetchPdfContentResultCallback callback = std::move(state->callback);

  // Note: PDF text extraction is currently restricted to top-level document
  // PDF only.
  base::UmaHistogramTimes(kPdfTextTopLevelLatencyHistogram,
                          elapsed_timer.Elapsed());

  // The sample is recorded in KB. Text extraction size is capped at 1MB by
  // default.
  base::UmaHistogramCounts10000(kPdfTextTopLevelSizeHistogram,
                                base::span(text).size_bytes() / 1024);

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

  // Truncate the `utf8_text` to the `text_byte_limit`.
  const bool truncated = TruncateUTF8ToByteLimit(utf8_text, text_byte_limit);
  const bool size_limit_exceeded = text.size() > text_byte_limit || truncated;

  base::UmaHistogramBoolean(kPdfTextTopLevelSizeLimitExceededHistogram,
                            size_limit_exceeded);

  // Move construct the PDF result.
  PdfResult result(std::move(pdf_origin), std::move(utf8_text));
  result.size_exceeded = size_limit_exceeded;
  std::move(callback).Run(std::move(result));
}

}  // namespace

void FetchPdfContentForWebContents(content::WebContents& web_contents,
                                   const PdfOptions& options,
                                   FetchPdfContentResultCallback callback) {
  base::ElapsedTimer elapsed_timer;
  // - For a top-level document PDF, the page's MIME type is `application/pdf`.
  // - For a page that embeds a PDF, for example, through an iframe whose `src`
  // points to a PDF URL, the page's MIME type is not `application/pdf`.
  bool is_top_level_pdf =
      web_contents.GetContentsMimeType() == pdf::kPDFMimeType;
  pdf::PDFDocumentHelper* pdf_helper = nullptr;

  if (is_top_level_pdf) {
    // For a top-level PDF, there is only one RenderFrameHost that renders the
    // PDF.
    pdf_helper = pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
  } else if (options.format() == PdfOptions::Format::kBytes &&
             base::FeatureList::IsEnabled(kGlicEmbeddedPdfBytesExtraction)) {
    // - This is not a top-level PDF.
    // - This is a bytes extraction request.
    // - The embedded PDF bytes extraction support feature is enabled.
    // Search for the `PDFDocumentHelper` associated with the embedded PDF.
    //
    // Note: Currently, the only requester for embedded PDF bytes is the Glic
    // API for page context. It ensures it only requests PDF bytes if the host
    // has the capability. See `WebClientInitialState::host_capabilities`.
    pdf_helper = GetPDFExtractionCandidate(web_contents);
  }

  if (options.format() == PdfOptions::Format::kBytes) {
    // This metric is specific to Glic, which requests PDF bytes only.
    RecordPdfRequestState(is_top_level_pdf,
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
  if (pdf_helper && pdf_helper->IsDocumentLoadComplete()) {
    auto state = base::MakeRefCounted<PdfExtractionState>(std::move(callback));
    switch (options.format()) {
      case PdfOptions::Format::kBytes: {
        pdf_helper->GetPdfBytes(
            options.size_limit(),
            mojo::WrapCallbackWithDropHandler(
                base::BindOnce(
                    &OnReceivedPdfBytes, state,
                    pdf_helper->render_frame_host().GetLastCommittedOrigin(),
                    is_top_level_pdf, options.size_limit(),
                    std::move(elapsed_timer)),
                base::BindOnce(&OnPdfPipeDisconnected, state)));
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
            mojo::WrapCallbackWithDropHandler(
                base::BindOnce(
                    &OnReceivedPdfText, state,
                    pdf_helper->render_frame_host().GetLastCommittedOrigin(),
                    options.size_limit(), std::move(elapsed_timer)),
                base::BindOnce(&OnPdfPipeDisconnected, state)));
        break;
      }
    }
    return;
  }

  if (options.format() == PdfOptions::Format::kText) {
    // The PDF text extraction is requested but not executed, record failure
    // status.
    // Note: PDF text extraction is currently restricted to top-level document
    // PDF only.
    // TODO(b/563402438): Unify the status metric for PDF bytes and text
    // extractions.
    if (!is_top_level_pdf) {
      RecordPdfTextExtractionStatus(PdfTextExtractionStatus::kNotPdf);
    } else if (!pdf_helper) {
      RecordPdfTextExtractionStatus(
          PdfTextExtractionStatus::kPdfExtractionNotAvailable);
    } else if (!pdf_helper->IsDocumentLoadComplete()) {
      RecordPdfTextExtractionStatus(
          PdfTextExtractionStatus::kPdfDocumentNotLoaded);
    }
  }

  std::move(callback).Run(std::nullopt);
}

pdf::PDFDocumentHelper* GetPDFExtractionCandidateForTesting(
    content::WebContents& contents) {
  return GetPDFExtractionCandidate(contents);
}

void ConvertPdfBytesToResultForTesting(
    url::Origin pdf_origin,
    bool is_top_level_pdf,
    uint32_t pdf_size_limit,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& pdf_bytes,
    uint32_t page_count,
    FetchPdfContentResultCallback callback) {
  OnReceivedPdfBytes(
      base::MakeRefCounted<PdfExtractionState>(std::move(callback)),
      std::move(pdf_origin), is_top_level_pdf, pdf_size_limit,
      base::ElapsedTimer(), status, pdf_bytes, page_count);
}

void ConvertPdfTextToResultForTesting(url::Origin pdf_origin,
                                      uint32_t text_byte_limit,
                                      const std::u16string& text,
                                      FetchPdfContentResultCallback callback) {
  OnReceivedPdfText(
      base::MakeRefCounted<PdfExtractionState>(std::move(callback)),
      std::move(pdf_origin), text_byte_limit, base::ElapsedTimer(), text);
}

}  // namespace page_content_annotations
