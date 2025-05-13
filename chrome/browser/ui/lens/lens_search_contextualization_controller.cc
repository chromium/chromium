// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/content_extraction/inner_html.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/lens_features.h"
#include "components/tabs/public/tab_interface.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

bool IsPageContextEligible(
    const GURL& main_frame_url,
    std::vector<optimization_guide::FrameMetadata> frame_metadata,
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  if (!page_context_eligibility ||
      !lens::features::IsLensSearchProtectedPageEnabled() ||
      !lens::features::IsLensOverlayContextualSearchboxEnabled() ||
      !lens::features::UseApcAsContext()) {
    return true;
  }
  return page_context_eligibility->api().IsPageContextEligible(
      main_frame_url.host(), main_frame_url.path(), std::move(frame_metadata));
}

}  // namespace

namespace lens {

LensSearchContextualizationController::LensSearchContextualizationController(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}
LensSearchContextualizationController::
    ~LensSearchContextualizationController() = default;

void LensSearchContextualizationController::StartContextualization(
    lens::LensOverlayInvocationSource invocation_source,
    lens::LensOverlayQueryController* lens_overlay_query_controller) {
  // TODO(crbug.com/403573362): Implement starting the query flow from here if
  // needed.
  return;
}

void LensSearchContextualizationController::GetPageContextualization(
    PageContentRetrievedCallback callback) {
  // If the contextual searchbox is disabled, exit early.
  if (!lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    std::move(callback).Run(/*page_contents=*/{}, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }

  is_page_context_eligible_ = true;
  lens_search_controller_->lens_overlay_side_panel_coordinator()
      ->SetShowProtectedErrorPage(false);

#if BUILDFLAG(ENABLE_PDF)
  // The overlay controller needs to check if the PDF helper exists before
  // calling MaybeGetPdfBytes or else the `callback` will have been moved but
  // not called.
  pdf::PDFDocumentHelper* pdf_helper =
      lens::features::UsePdfsAsContext()
          ? pdf::PDFDocumentHelper::MaybeGetForWebContents(
                lens_search_controller_->GetTabInterface()->GetContents())
          : nullptr;
  if (lens::features::UsePdfsAsContext() && pdf_helper) {
    // Fetch the PDF bytes then run the callback.
    MaybeGetPdfBytes(pdf_helper, std::move(callback));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  std::vector<lens::PageContent> page_contents;
  auto* render_frame_host = lens_search_controller_->GetTabInterface()
                                ->GetContents()
                                ->GetPrimaryMainFrame();
  if (!render_frame_host || (!lens::features::UseInnerHtmlAsContext() &&
                             !lens::features::UseInnerTextAsContext() &&
                             !lens::features::UseApcAsContext())) {
    std::move(callback).Run(page_contents, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }
  // TODO(crbug.com/399610478): The fetches for innerHTML, innerText, and APC
  // should be parallelized to fetch all data at once. Currently fetches are
  // sequential to prevent getting stuck in a race condition.
  MaybeGetInnerHtml(page_contents, render_frame_host, std::move(callback));
}

#if BUILDFLAG(ENABLE_PDF)
void LensSearchContextualizationController::
    FetchVisiblePageIndexAndGetPartialPdfText(
        lens::LensOverlayQueryController* lens_overlay_query_controller,
        uint32_t page_count,
        PdfPartialPageTextRetrievedCallback callback) {
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          lens_search_controller_->GetTabInterface()->GetContents());
  if (!pdf_helper ||
      lens::features::GetLensOverlayPdfSuggestCharacterTarget() == 0 ||
      page_count == 0) {
    return;
  }

  CHECK(lens_overlay_query_controller);
  CHECK(callback);
  lens_overlay_query_controller_ = lens_overlay_query_controller;
  pdf_partial_page_text_retrieved_callback_ = std::move(callback);

  // TODO(387306854): Add logic to grab page text form the visible page index.

  // Fetch the first page of text which will be then recursively fetch following
  // pages.
  pdf_pages_text_.clear();
  pdf_helper->GetPageText(
      /*page_index=*/0,
      base::BindOnce(
          &LensSearchContextualizationController::GetPartialPdfTextCallback,
          weak_ptr_factory_.GetWeakPtr(), /*page_index=*/0, page_count,
          /*total_characters_retrieved=*/0));
}
#endif  // BUILDFLAG(ENABLE_PDF)

void LensSearchContextualizationController::MaybeGetInnerHtml(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseInnerHtmlAsContext()) {
    MaybeGetInnerText(page_contents, render_frame_host, std::move(callback));
    return;
  }
  content_extraction::GetInnerHtml(
      *render_frame_host,
      base::BindOnce(
          &LensSearchContextualizationController::OnInnerHtmlReceived,
          weak_ptr_factory_.GetWeakPtr(), page_contents, render_frame_host,
          std::move(callback)));
}

void LensSearchContextualizationController::OnInnerHtmlReceived(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback,
    const std::optional<std::string>& result) {
  const bool was_successful =
      result.has_value() &&
      result->size() <= lens::features::GetLensOverlayFileUploadLimitBytes();
  // Add the innerHTML to the page contents if successful, or empty bytes if
  // not.
  page_contents.emplace_back(
      /*bytes=*/was_successful
          ? std::vector<uint8_t>(result->begin(), result->end())
          : std::vector<uint8_t>{},
      lens::MimeType::kHtml);
  MaybeGetInnerText(page_contents, render_frame_host, std::move(callback));
}

void LensSearchContextualizationController::MaybeGetInnerText(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseInnerTextAsContext()) {
    MaybeGetAnnotatedPageContent(page_contents, render_frame_host,
                                 std::move(callback));
    return;
  }
  content_extraction::GetInnerText(
      *render_frame_host, /*node_id=*/std::nullopt,
      base::BindOnce(
          &LensSearchContextualizationController::OnInnerTextReceived,
          weak_ptr_factory_.GetWeakPtr(), page_contents, render_frame_host,
          std::move(callback)));
}

void LensSearchContextualizationController::OnInnerTextReceived(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  const bool was_successful =
      result && result->inner_text.size() <=
                    lens::features::GetLensOverlayFileUploadLimitBytes();
  // Add the innerText to the page_contents if successful, or empty bytes if
  // not.
  page_contents.emplace_back(
      /*bytes=*/was_successful
          ? std::vector<uint8_t>(result->inner_text.begin(),
                                 result->inner_text.end())
          : std::vector<uint8_t>{},
      lens::MimeType::kPlainText);
  MaybeGetAnnotatedPageContent(page_contents, render_frame_host,
                               std::move(callback));
}

void LensSearchContextualizationController::MaybeGetAnnotatedPageContent(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseApcAsContext()) {
    // Done fetching page contents.
    // Keep legacy behavior consistent by setting the primary content type to
    // plain text if that is the only content type enabled.
    // TODO(crbug.com/401614601): Set primary content type to kHtml in all
    // cases.
    auto primary_content_type = lens::features::UseInnerTextAsContext() &&
                                        !lens::features::UseInnerHtmlAsContext()
                                    ? lens::MimeType::kPlainText
                                    : lens::MimeType::kHtml;
    std::move(callback).Run(page_contents, primary_content_type, std::nullopt);
    return;
  }

  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions();
  ai_page_content_options->on_critical_path = true;
  ai_page_content_options->max_meta_elements = 20;
  optimization_guide::GetAIPageContent(
      lens_search_controller_->GetTabInterface()->GetContents(),
      std::move(ai_page_content_options),
      base::BindOnce(&LensSearchContextualizationController::
                         OnAnnotatedPageContentReceived,
                     weak_ptr_factory_.GetWeakPtr(), page_contents,
                     std::move(callback)));
}

void LensSearchContextualizationController::OnAnnotatedPageContentReceived(
    std::vector<lens::PageContent> page_contents,
    PageContentRetrievedCallback callback,
    std::optional<optimization_guide::AIPageContentResult> result) {
  // Add the apc proto the page_contents if it exists.
  if (result) {
    // Convert the page metadata to a C struct defined in the optimization_guide
    // component so it can be passed to the shared library.
    std::vector<optimization_guide::FrameMetadata> frame_metadata_structs =
        lens::ConvertFrameMetadataFromProto(result.value());

    // If the page is protected, do not send the latest page content to the
    // server.
    const auto& tab_url = lens_search_controller_->GetTabInterface()
                              ->GetContents()
                              ->GetLastCommittedURL();
    if (!IsPageContextEligible(
            tab_url, std::move(frame_metadata_structs),
            lens_search_controller_->page_context_eligibility())) {
      is_page_context_eligible_ = false;
      lens_search_controller_->lens_overlay_side_panel_coordinator()
          ->SetShowProtectedErrorPage(true);
      // Clear all previous page contents.
      page_contents.clear();
    } else {
      std::string serialized_apc;
      result->proto.SerializeToString(&serialized_apc);
      page_contents.emplace_back(
          std::vector<uint8_t>(serialized_apc.begin(), serialized_apc.end()),
          lens::MimeType::kAnnotatedPageContent);
    }
  }
  // Done fetching page contents.
  std::move(callback).Run(page_contents, lens::MimeType::kAnnotatedPageContent,
                          std::nullopt);
}

#if BUILDFLAG(ENABLE_PDF)
void LensSearchContextualizationController::MaybeGetPdfBytes(
    pdf::PDFDocumentHelper* pdf_helper,
    PageContentRetrievedCallback callback) {
  // Try and fetch the PDF bytes if enabled.
  CHECK(pdf_helper);
  pdf_helper->GetPdfBytes(
      /*size_limit=*/lens::features::GetLensOverlayFileUploadLimitBytes(),
      base::BindOnce(&LensSearchContextualizationController::OnPdfBytesReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LensSearchContextualizationController::OnPdfBytesReceived(
    PageContentRetrievedCallback callback,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  // TODO(crbug.com/370530197): Show user error message if status is not
  // success.
  if (status != pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess ||
      page_count == 0) {
    std::move(callback).Run(
        {lens::PageContent(/*bytes=*/{}, lens::MimeType::kPdf)},
        lens::MimeType::kPdf, page_count);
    return;
  }
  std::move(callback).Run({lens::PageContent(bytes, lens::MimeType::kPdf)},
                          lens::MimeType::kPdf, page_count);
}

void LensSearchContextualizationController::GetPartialPdfTextCallback(
    uint32_t page_index,
    uint32_t total_page_count,
    uint32_t total_characters_retrieved,
    const std::u16string& page_text) {
  // Sanity checks that the input is expected.
  CHECK_GE(total_page_count, 1u);
  CHECK_LT(page_index, total_page_count);
  CHECK_EQ(pdf_pages_text_.size(), page_index);
  CHECK(lens_overlay_query_controller_);

  // Add the page text to the list of pages and update the total characters
  // retrieved count.
  pdf_pages_text_.push_back(page_text);

  // Ensure no integer overflow. If overflow, set the total characters retrieved
  // to the max value so the loop will exit.
  base::CheckedNumeric<uint32_t> total_characters_retrieved_check =
      total_characters_retrieved;
  total_characters_retrieved_check += page_text.size();
  total_characters_retrieved = total_characters_retrieved_check.ValueOrDefault(
      std::numeric_limits<uint32_t>::max());

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          lens_search_controller_->GetTabInterface()->GetContents());

  // Stop the loop if the character limit is reached or if the page index is
  // out of bounds or the PDF helper no longer exists.
  if (!pdf_helper ||
      total_characters_retrieved >=
          lens::features::GetLensOverlayPdfSuggestCharacterTarget() ||
      page_index + 1 >= total_page_count) {
    std::move(pdf_partial_page_text_retrieved_callback_).Run(pdf_pages_text_);
    lens_overlay_query_controller_->SendPartialPageContentRequest(
        pdf_pages_text_);
    // Reset the query controller to prevent dangling pointer.
    lens_overlay_query_controller_ = nullptr;
    return;
  }

  pdf_helper->GetPageText(
      page_index + 1,
      base::BindOnce(
          &LensSearchContextualizationController::GetPartialPdfTextCallback,
          weak_ptr_factory_.GetWeakPtr(), page_index + 1, total_page_count,
          total_characters_retrieved));
}
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace lens
