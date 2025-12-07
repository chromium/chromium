// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/content_extraction/inner_html.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/lens/lens_features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/skia_util.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

// The amount of change in bytes that is considered a significant change and
// should trigger a page content update request. This provides tolerance in
// case there is slight variation in the retrieved bytes in between calls.
constexpr float kByteChangeTolerancePercent = 0.01;

// The maximum length of the DOM text to consider for OCR similarity.
// Currently 50 MB
constexpr int kMaxDomTextLengthForOcrSimilarity = 50 * 1000 * 1000;

// Returns a new string with all non-alphanumeric characters removed from the
// ends of the string.
std::string TrimNonAlphaNumeric(const std::string& text) {
  if (text.empty()) {
    return text;
  }

  // Find the first alphanumeric character from the beginning.
  size_t first_alphanum_index =
      std::find_if(text.begin(), text.end(), ::isalnum) - text.begin();

  // If no alphanumeric character is found in the entire string, return an empty
  // string.
  if (first_alphanum_index == text.length()) {
    return "";
  }

  // Find the index of the last alphanumeric character from the end.
  size_t last_alphanum_index =
      std::find_if(text.rbegin(), text.rend(), ::isalnum) - text.rbegin();
  // `last_alphanumeric` is the count from the end of the string, so convert to
  // index from the beginning.
  last_alphanum_index = text.length() - 1 - last_alphanum_index;

  // Extract the substring containing only the alphanumeric characters and those
  // in between.
  return text.substr(first_alphanum_index,
                     last_alphanum_index - first_alphanum_index + 1);
}

// Returns the percentage of words in the OCR text that are also in the DOM
// text.
double CalculateWordOverlapSimilarity(std::string dom_text,
                                      lens::mojom::TextPtr ocr_text) {
  // Split dom_text into possible words.
  std::vector<std::string> dom_words = base::SplitString(
      dom_text, " \t\r\n<>", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Convert dom_text to lowercase, alphanumeric only map for comparison. The
  // map value is the number of times the word appears in the dom text.
  std::map<std::string, int> dom_words_map;
  for (std::string& word : dom_words) {
    std::string processed_word = TrimNonAlphaNumeric(base::ToLowerASCII(word));
    if (!processed_word.empty()) {
      dom_words_map[processed_word]++;
    }
  }

  // Count the number of words in ocr_text that are also in the dom text.
  double overlap_count = 0;
  double total_ocr_words = 0;
  if (ocr_text && ocr_text->text_layout &&
      ocr_text->text_layout->paragraphs.size() > 0) {
    for (const auto& paragraph : ocr_text->text_layout->paragraphs) {
      if (paragraph && paragraph->lines.size() > 0) {
        for (const auto& line : paragraph->lines) {
          if (line && line->words.size() > 0) {
            for (const auto& word : line->words) {
              if (word) {
                std::string processed_word =
                    TrimNonAlphaNumeric(base::ToLowerASCII(word->plain_text));
                if (processed_word.empty()) {
                  continue;
                }

                // Find the process word in the dom words.
                auto word_iterator = dom_words_map.find(processed_word);
                if (word_iterator != dom_words_map.end() &&
                    word_iterator->second > 0) {
                  // The word is in the dom text.
                  overlap_count++;

                  // Decrement the count in the map so if there are multiple of
                  // this word in the DOM, we only count it for each instance.
                  word_iterator->second--;
                }
                total_ocr_words++;
              }
            }
          }
        }
      }
    }
  }

  // Avoid divide by zero. Return the percentage of words in the OCR text that
  // are also in the DOM text.
  return total_ocr_words == 0 ? 0.0 : overlap_count / total_ocr_words;
}

bool IsProtectedPageFeatureEnabled() {
  return lens::features::IsLensSearchProtectedPageEnabled() &&
         lens::IsLensOverlayContextualSearchboxEnabled() &&
         lens::features::UseApcAsContext();
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
    OnPageContextUpdatedCallback callback) {
  CHECK(state_ == State::kOff);
  state_ = State::kInitializing;
  invocation_source_ = invocation_source;
  // TODO(crbug.com/403573362): Implement starting the query flow from here if
  // needed.
  StartScreenshotFlow(base::BindOnce(
      &LensSearchContextualizationController::OnScreenshotTakenForContextual,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LensSearchContextualizationController::GetPageContextualization(
    PageContentRetrievedCallback callback) {
  // If the contextual searchbox is disabled, exit early.
  if (!lens::IsLensOverlayContextualSearchboxEnabled()) {
    std::move(callback).Run(/*page_contents=*/{}, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }

  is_page_context_eligible_ = true;

#if BUILDFLAG(ENABLE_PDF)
  // The overlay controller needs to check if the PDF helper exists before
  // calling MaybeGetPdfBytes or else the `callback` will have been moved but
  // not called.
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          lens_search_controller_->GetTabInterface()->GetContents());
  if (pdf_helper) {
    // Fetch the PDF bytes then run the callback.
    MaybeGetPdfBytes(pdf_helper, std::move(callback));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  std::vector<lens::PageContent> page_contents;
  auto* render_frame_host = lens_search_controller_->GetTabInterface()
                                ->GetContents()
                                ->GetPrimaryMainFrame();
  if (!render_frame_host || (!lens::features::UseInnerTextAsContext() &&
                             !lens::features::UseApcAsContext())) {
    std::move(callback).Run(page_contents, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }
  // TODO(crbug.com/399610478): The fetches for innerText and APC
  // should be parallelized to fetch all data at once. Currently fetches are
  // sequential to prevent getting stuck in a race condition.
  MaybeGetInnerText(page_contents, render_frame_host, std::move(callback));
}

void LensSearchContextualizationController::TryUpdatePageContextualization(
    OnPageContextUpdatedCallback callback) {
  if (state_ == State::kInitializing) {
    // Will be called again by OnInitialPageContextEligibilityFetched when the
    // controller finishes initializing.
    return;
  }
  if (state_ == State::kOff) {
    // TODO(crbug.com/418825720): The viewport screenshot should be only be set
    // in this controller in the future.
    viewport_screenshot_ = lens_search_controller_->lens_overlay_controller()
                               ->initial_screenshot();
    state_ = State::kActive;
  }
  CHECK(state_ == State::kActive);

  // If there is already an upload, do not send another request.
  // TODO(crbug.com/399154548): Ideally, there could be two uploads in progress
  // at a time, however, the current query controller implementation does not
  // support this.
  if (GetQueryController()->IsPageContentUploadInProgress()) {
    std::move(callback).Run();
    return;
  }

  on_page_context_updated_callback_ = std::move(callback);
  GetPageContextualization(base::BindOnce(
      &LensSearchContextualizationController::UpdatePageContextualization,
      weak_ptr_factory_.GetWeakPtr()));
}

#if BUILDFLAG(ENABLE_PDF)
void LensSearchContextualizationController::
    FetchVisiblePageIndexAndGetPartialPdfText(
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
  CHECK(callback);
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

void LensSearchContextualizationController::ResetState() {
  on_page_context_updated_callback_.Reset();
  is_page_context_eligible_ = false;
  ocr_dom_similarity_recorded_in_session_ = false;
  page_contents_.clear();
  primary_content_type_ = lens::MimeType::kUnknown;
  viewport_screenshot_.reset();
  last_retrieved_most_visible_page_ = std::nullopt;
  pdf_partial_page_text_retrieved_callback_.Reset();
  pdf_pages_text_.clear();
  // Reset the page context eligibility API state.
  page_context_eligibility_callback_.Reset();
  pending_context_eligibility_params_.reset();
  state_ = State::kOff;
}

void LensSearchContextualizationController::SetPageContent(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type) {
  page_contents_ = page_contents;
  primary_content_type_ = primary_content_type;
}

void LensSearchContextualizationController::RecordDocumentMetrics(
    std::optional<uint32_t> page_count) {
  // Record the document size bytes for each lens::PageContent. If there are no
  // page contents, then we will record 0.
  std::set<lens::MimeType> retrieved_content_types;
  if (page_contents_.empty()) {
    lens::RecordDocumentSizeBytes(lens::MimeType::kUnknown, 0);
  } else {
    for (const auto& page_content : page_contents_) {
      lens::RecordDocumentSizeBytes(page_content.content_type_,
                                    page_content.bytes_.size());
      retrieved_content_types.insert(page_content.content_type_);
    }
  }

  if (page_count.has_value() && primary_content_type_ == lens::MimeType::kPdf) {
    lens::RecordPdfPageCount(page_count.value());
    return;
  }

  // Fetch and record the other content type for representing the webpage.
  // TODO(crbug.com/398304347): Remove this once innerText metrics are recorded
  // as part of the content data.
  auto* render_frame_host = lens_search_controller_->GetTabInterface()
                                ->GetContents()
                                ->GetPrimaryMainFrame();
  if (!retrieved_content_types.contains(lens::MimeType::kPlainText)) {
    // Fetch the innerText to log the size.
    content_extraction::GetInnerText(
        *render_frame_host, /*node_id=*/std::nullopt,
        base::BindOnce(
            &LensSearchContextualizationController::RecordInnerTextSize,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // Try and record the OCR DOM similarity since the page content is now
  // available.
  TryCalculateAndRecordOcrDomSimilarity();
}

void LensSearchContextualizationController::
    TryCalculateAndRecordOcrDomSimilarity() {
  // Exit early if we do not have all the data needed to calculate the
  // similarity.
  if (!text_ || page_contents_.empty() ||
      ocr_dom_similarity_recorded_in_session_) {
    return;
  }
  ocr_dom_similarity_recorded_in_session_ = true;

  const auto& page_content_bytes = page_contents_.front().bytes_;

  const auto primary_content_type = primary_content_type_;
  bool is_dom = primary_content_type == lens::MimeType::kHtml ||
                primary_content_type == lens::MimeType::kPlainText ||
                primary_content_type == lens::MimeType::kAnnotatedPageContent;
  bool is_dom_too_large =
      page_content_bytes.size() > kMaxDomTextLengthForOcrSimilarity;
  bool is_english = text_->content_language == "en";

  // Exit early if the page content is not from the DOM, the DOM is very large
  // and might bog down the thread, or the page is not in English since the
  // score is not reliable for other languages.
  if (!is_dom || is_dom_too_large || !is_english) {
    // If the page content is not from the HTML DOM, the similarity cannot be
    // calculated, so reset the text to avoid trying again.
    text_.reset();
    return;
  }

  // Post to a background thread to calculate the similarity to avoid slowing
  // down the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &CalculateWordOverlapSimilarity,
          std::string(page_content_bytes.begin(), page_content_bytes.end()),
          text_.Clone()),
      base::BindOnce(&lens::RecordOcrDomSimilarity));
}

void LensSearchContextualizationController::SetText(lens::mojom::TextPtr text) {
  text_ = std::move(text);
}

void LensSearchContextualizationController::UpdatePageContextualization(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count) {
  // Exit early if the controller is off.
  if (state_ == State::kOff) {
    return;
  }

  if (!lens::IsLensOverlayContextualSearchboxEnabled()) {
    std::move(on_page_context_updated_callback_).Run();
    return;
  }

  // If page is not eligible, then return early as none of the content
  // will be sent.
  if (!is_page_context_eligible_) {
    std::move(on_page_context_updated_callback_).Run();
    return;
  }

  // Do not capture a new screenshot if the feature param is not enabled or if
  // the overlay is showing over the live page, meaning the viewport cannot have
  // changed.
  if (!lens::features::UpdateViewportEachQueryEnabled() ||
      lens_search_controller_->lens_overlay_controller()->IsOverlayShowing()) {
    UpdatePageContextualizationPart2(page_contents, primary_content_type,
                                     page_count, SkBitmap());
    return;
  }

  // Begin the process of grabbing a screenshot.
  CaptureScreenshot(base::BindOnce(
      &LensSearchContextualizationController::UpdatePageContextualizationPart2,
      weak_ptr_factory_.GetWeakPtr(), page_contents, primary_content_type,
      page_count));
}

void LensSearchContextualizationController::UpdatePageContextualizationPart2(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count,
    const SkBitmap& bitmap) {
  // It's possible the Lens session could have been closed while updating the
  // page context. Return early and do not run the callback as it should have
  // been cleared.
  if (state_ == State::kOff || !on_page_context_updated_callback_) {
    return;
  }

#if BUILDFLAG(ENABLE_PDF)
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          lens_search_controller_->GetTabInterface()->GetContents());
  if (pdf_helper) {
    pdf_helper->GetMostVisiblePageIndex(base::BindOnce(
        &LensSearchContextualizationController::UpdatePageContext,
        weak_ptr_factory_.GetWeakPtr(), page_contents, primary_content_type,
        page_count, bitmap));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  UpdatePageContext(page_contents, primary_content_type, page_count, bitmap,
                    /*most_visible_page=*/std::nullopt);
}

void LensSearchContextualizationController::UpdatePageContext(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count,
    const SkBitmap& bitmap,
    std::optional<uint32_t> most_visible_page) {
  // It's possible the Lens session could have been closed while updating the
  // page context. Return early and do not run the callback as it should have
  // been cleared.
  if (state_ == State::kOff) {
    return;
  }

  bool sending_bitmap = false;
  if (!bitmap.drawsNothing() &&
      (viewport_screenshot_.drawsNothing() ||
       !gfx::BitmapsAreEqual(viewport_screenshot_, bitmap))) {
    viewport_screenshot_ = bitmap;
    sending_bitmap = true;

    // If the overlay is NOT showing/initializing, then the selections should be
    // cleared so future contextual queries do not include it. The thumbnail
    // will be updated by the query controller on region searches if needed.
    if (!lens_search_controller_->lens_overlay_controller()
             ->IsOverlayShowing() &&
        !lens_search_controller_->lens_overlay_controller()
             ->IsOverlayInitializing()) {
      lens_search_controller_->lens_overlay_controller()->ClearAllSelections();
      lens_search_controller_->HandleThumbnailCreatedBitmap(bitmap);
    }
  }
  last_retrieved_most_visible_page_ = most_visible_page;

  // TODO(crbug.com/399215935): Ideally, this check should ensure that any of
  // the content date has not changed. For now, we only check if the
  // primary_content_type bytes have changed.
  auto old_page_content_it = std::ranges::find_if(
      page_contents_, [&primary_content_type](const auto& page_content) {
        return page_content.content_type_ == primary_content_type;
      });
  auto new_page_content_it = std::ranges::find_if(
      page_contents, [&primary_content_type](const auto& page_content) {
        return page_content.content_type_ == primary_content_type;
      });
  const lens::PageContent* old_page_content =
      old_page_content_it != page_contents_.end() ? &(*old_page_content_it)
                                                  : nullptr;
  const lens::PageContent* new_page_content =
      new_page_content_it != page_contents.end() ? &(*new_page_content_it)
                                                 : nullptr;

  if (primary_content_type_ == primary_content_type && old_page_content &&
      new_page_content) {
    const float old_size = old_page_content->bytes_.size();
    const float new_size = new_page_content->bytes_.size();
    const float percent_changed = abs((new_size - old_size) / old_size);
    if (percent_changed < kByteChangeTolerancePercent) {
      if (!sending_bitmap) {
        // If the bytes have not changed more than our threshold and the
        // screenshot has not changed, exit early. Notify the query controller
        // that the user may be issuing a search request, and therefore the
        // query should be restarted if TTL expired. If the bytes did change,
        // this will happen automatically as a result of the
        // SendUpdatedPageContent call below.
        GetQueryController()->MaybeRestartQueryFlow();
        if (on_page_context_updated_callback_) {
          std::move(on_page_context_updated_callback_).Run();
        }
        return;
      }

      // If the screenshot has changed but the bytes have not, send only the
      // screenshot.
      GetQueryController()->SendUpdatedPageContent(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt,
          last_retrieved_most_visible_page_,
          sending_bitmap ? bitmap : SkBitmap());

      // Run the callback that the page context has finished updating.
      if (on_page_context_updated_callback_) {
        std::move(on_page_context_updated_callback_).Run();
      }
      return;
    }
  }

  // Since the page content has changed, let the query controller know to avoid
  // dangling pointers.
  GetQueryController()->ResetPageContentData();

  page_contents_ = page_contents;
  primary_content_type_ = primary_content_type;

  // If no bytes were retrieved from the page, the query won't be able to be
  // contextualized. Notify the side panel so the ghost loader isn't shown. No
  // need to update update the overlay as this update only happens on navigation
  // where the side panel will already be open.
  if (!new_page_content || new_page_content->bytes_.empty()) {
    lens_search_controller_->lens_overlay_side_panel_coordinator()
        ->SuppressGhostLoader();
  }

#if BUILDFLAG(ENABLE_PDF)
  // If the new page is a PDF, fetch the text from the page to be used as early
  // suggest signals.
  if (new_page_content &&
      new_page_content->content_type_ == lens::MimeType::kPdf) {
    FetchVisiblePageIndexAndGetPartialPdfText(
        page_count.value_or(0),
        base::BindOnce(&LensSearchContextualizationController::
                           OnPdfPartialPageTextRetrieved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
#endif

  GetQueryController()->SendUpdatedPageContent(
      page_contents_, primary_content_type_,
      lens_search_controller_->GetPageURL(),
      lens_search_controller_->GetPageTitle(),
      last_retrieved_most_visible_page_, sending_bitmap ? bitmap : SkBitmap());
  RecordDocumentMetrics(page_count.value_or(0));
  lens_search_controller_->lens_session_metrics_logger()
      ->OnFollowUpPageContentRetrieved(primary_content_type);

  // Run the callback that the page context has finished updating.
  if (on_page_context_updated_callback_) {
    std::move(on_page_context_updated_callback_).Run();
  }
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
    auto primary_content_type = lens::features::UseInnerTextAsContext()
                                    ? lens::MimeType::kPlainText
                                    : lens::MimeType::kHtml;
    std::move(callback).Run(page_contents, primary_content_type, std::nullopt);
    return;
  }

  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path =*/true);
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
    optimization_guide::AIPageContentResultOrError result) {
  // The tab URL is used to check if the page is context eligible.
  const auto& tab_url = lens_search_controller_->GetTabInterface()
                            ->GetContents()
                            ->GetLastCommittedURL();

  // Add the apc proto the page_contents if it exists.
  if (result.has_value()) {
    // Convert the page metadata to a C struct defined in the optimization_guide
    // component so it can be passed to the shared library.
    std::vector<optimization_guide::FrameMetadata> frame_metadata_structs =
        optimization_guide::GetFrameMetadataFromPageContent(result.value());

    // If the page is protected, do not send the latest page content to the
    // server.
    IsPageContextEligible(
        tab_url, std::move(frame_metadata_structs),
        base::BindOnce(&LensSearchContextualizationController::
                           OnPageContextEligibilityFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::move(page_contents),
                       std::move(callback), std::move(result.value())));
    return;
  }

  IsPageContextEligible(
      tab_url, {},
      base::BindOnce(&LensSearchContextualizationController::
                         OnPageContextEligibilityFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(page_contents),
                     std::move(callback), std::nullopt));
}

void LensSearchContextualizationController::OnPageContextEligibilityFetched(
    std::vector<lens::PageContent> page_contents,
    PageContentRetrievedCallback callback,
    std::optional<optimization_guide::AIPageContentResult> result,
    bool is_page_context_eligible) {
  if (!is_page_context_eligible) {
    is_page_context_eligible_ = false;
    lens_search_controller_->lens_overlay_side_panel_coordinator()
        ->SetShowProtectedErrorPage(true);
    // Clear all previous page contents.
    page_contents.clear();
  } else if (result) {
    std::string serialized_apc;
    result->proto.SerializeToString(&serialized_apc);
    page_contents.emplace_back(
        std::vector<uint8_t>(serialized_apc.begin(), serialized_apc.end()),
        lens::MimeType::kAnnotatedPageContent);
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
    GetQueryController()->SendPartialPageContentRequest(pdf_pages_text_);
    return;
  }

  pdf_helper->GetPageText(
      page_index + 1,
      base::BindOnce(
          &LensSearchContextualizationController::GetPartialPdfTextCallback,
          weak_ptr_factory_.GetWeakPtr(), page_index + 1, total_page_count,
          total_characters_retrieved));
}

void LensSearchContextualizationController::OnPdfPartialPageTextRetrieved(
    std::vector<std::u16string> pdf_pages_text) {
  pdf_pages_text_ = std::move(pdf_pages_text);
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool LensSearchContextualizationController::IsScreenshotPossible(
    content::RenderWidgetHostView* view) {
  return view && view->IsSurfaceAvailableForCopy();
}

void LensSearchContextualizationController::StartScreenshotFlow(
    OnScreenshotTakenCallback callback) {
  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view =
      lens_search_controller_->GetTabInterface()
          ->GetContents()
          ->GetPrimaryMainFrame()
          ->GetRenderViewHost()
          ->GetWidget()
          ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!IsScreenshotPossible(view)) {
    std::move(callback).Run(SkBitmap(), {}, std::nullopt);
    return;
  }

  // Side panel is now fully closed, take screenshot and open overlay.
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&LensSearchContextualizationController::
                             FetchViewportImageBoundingBoxes,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void LensSearchContextualizationController::CaptureScreenshot(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view =
      lens_search_controller_->GetTabInterface()
          ->GetContents()
          ->GetPrimaryMainFrame()
          ->GetRenderViewHost()
          ->GetWidget()
          ->GetView();

  if (!IsScreenshotPossible(view)) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&LensSearchContextualizationController::
                             OnScreenshotCapturedForUpdate,
                         weak_ptr_factory_.GetWeakPtr(),
                         ++screenshot_attempt_id_, std::move(callback))));
}

void LensSearchContextualizationController::OnScreenshotCapturedForUpdate(
    int attempt_id,
    base::OnceCallback<void(const SkBitmap&)> callback,
    const viz::CopyOutputBitmapWithMetadata& result) {
  if (attempt_id != screenshot_attempt_id_) {
    return;
  }
  std::move(callback).Run(result.bitmap);
}

void LensSearchContextualizationController::DidCaptureScreenshot(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int attempt_id,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& bounds,
    OnScreenshotTakenCallback callback,
    std::optional<uint32_t> pdf_current_page) {
  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  if (bitmap.drawsNothing()) {
    std::move(callback).Run(SkBitmap(), {}, std::nullopt);
    lens_search_controller_->CloseLensSync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  std::move(callback).Run(bitmap, bounds, pdf_current_page);
}

void LensSearchContextualizationController::OnScreenshotTakenForContextual(
    OnPageContextUpdatedCallback callback,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page) {
  // Start the query as soon as the image is ready since it is the only
  // critical asynchronous flow. This optimization parallelizes the query flow
  // with other async startup processes.
  const auto& tab_url = lens_search_controller_->GetTabInterface()
                            ->GetContents()
                            ->GetLastCommittedURL();

  // Check if the page is context eligible. This should start the query flow
  // after the eligibility is fetched.
  IsPageContextEligible(
      tab_url, /*frame_metadata=*/{},
      base::BindOnce(&LensSearchContextualizationController::
                         OnInitialPageContextEligibilityFetched,
                     weak_ptr_factory_.GetWeakPtr(), bitmap, all_bounds,
                     pdf_current_page, std::move(callback)));
}

void LensSearchContextualizationController::IsPageContextEligible(
    const GURL& main_frame_url,
    std::vector<optimization_guide::FrameMetadata> frame_metadata,
    LensSearchPageContextEligibilityCallback callback) {
  if (!IsProtectedPageFeatureEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  if (!page_context_eligibility_) {
    // If the page context eligibility API failed to load, then the page should
    // be marked as not eligible.
    if (has_page_context_eligibility_api_loaded_) {
      std::move(callback).Run(false);
      return;
    }

    // If the page context eligibility API is not yet available, then wait for
    // it to be loaded before checking eligibility by storing the callback and
    // checking again once the API is loaded.
    pending_context_eligibility_params_.emplace(main_frame_url,
                                                std::move(frame_metadata));
    page_context_eligibility_callback_ = std::move(callback);
    return;
  }

  std::move(callback).Run(optimization_guide::IsPageContextEligible(
      main_frame_url.GetHost(), main_frame_url.GetPath(),
      std::move(frame_metadata), page_context_eligibility_));
}

void LensSearchContextualizationController::CreatePageContextEligibilityAPI() {
  // Post to a background thread to avoid blocking the set up of the overlay.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(&LensSearchContextualizationController::
                         OnPageContextEligibilityAPILoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool LensSearchContextualizationController::GetCurrentPageContextEligibility() {
  if (!IsProtectedPageFeatureEnabled()) {
    return true;
  }

  return is_page_context_eligible_ && has_page_context_eligibility_api_loaded_;
}

LensSearchContextualizationController::PageContextEligibilityParams::
    PageContextEligibilityParams(
        const GURL& main_frame_url,
        std::vector<optimization_guide::FrameMetadata> frame_metadata)
    : main_frame_url(main_frame_url),
      frame_metadata(std::move(frame_metadata)) {}

LensSearchContextualizationController::PageContextEligibilityParams::
    ~PageContextEligibilityParams() = default;

void LensSearchContextualizationController::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
  has_page_context_eligibility_api_loaded_ = true;
  if (page_context_eligibility_callback_ &&
      pending_context_eligibility_params_) {
    std::move(page_context_eligibility_callback_)
        .Run(optimization_guide::IsPageContextEligible(
            pending_context_eligibility_params_->main_frame_url.GetHost(),
            pending_context_eligibility_params_->main_frame_url.GetPath(),
            std::move(pending_context_eligibility_params_->frame_metadata),
            page_context_eligibility_));
    pending_context_eligibility_params_.reset();
  }
}

void LensSearchContextualizationController::
    OnInitialPageContextEligibilityFetched(
        const SkBitmap& bitmap,
        const std::vector<gfx::Rect>& all_bounds,
        std::optional<uint32_t> pdf_current_page,
        OnPageContextUpdatedCallback callback,
        bool is_page_context_eligible) {
  auto bitmap_to_send = bitmap;
  auto page_url = lens_search_controller_->GetPageURL();
  auto page_title = lens_search_controller_->GetPageTitle();
  if (!is_page_context_eligible) {
    is_page_context_eligible_ = false;
    lens_search_controller_->lens_overlay_side_panel_coordinator()
        ->SetShowProtectedErrorPage(true);
    bitmap_to_send = SkBitmap();
    page_url = GURL();
    page_title = "";
  }

  viewport_screenshot_ = bitmap_to_send;
  page_url_ = page_url;
  page_title_ = page_title;

  GetQueryController()->StartQueryFlow(
      viewport_screenshot_, page_url_, page_title_,
      ConvertSignificantRegionBoxes(all_bounds),
      std::vector<lens::PageContent>(), lens::MimeType::kUnknown,
      pdf_current_page, GetUiScaleFactor(), base::TimeTicks::Now());

  // Pass the thumbnail to the searchbox controller.
  lens_search_controller_->HandleThumbnailCreatedBitmap(bitmap_to_send);

  state_ = State::kActive;
  TryUpdatePageContextualization(std::move(callback));
}

void LensSearchContextualizationController::FetchViewportImageBoundingBoxes(
    OnScreenshotTakenCallback callback,
    const viz::CopyOutputBitmapWithMetadata& result) {
  const SkBitmap& bitmap = result.bitmap;
  content::RenderFrameHost* render_frame_host =
      lens_search_controller_->GetTabInterface()
          ->GetContents()
          ->GetPrimaryMainFrame();
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  // Bind the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  auto* frame = chrome_render_frame.get();

  frame->RequestBoundsHintForAllImages(base::BindOnce(
      &LensSearchContextualizationController::GetPdfCurrentPage,
      weak_ptr_factory_.GetWeakPtr(), std::move(chrome_render_frame),
      ++screenshot_attempt_id_, bitmap, std::move(callback)));
}

void LensSearchContextualizationController::GetPdfCurrentPage(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int attempt_id,
    const SkBitmap& bitmap,
    OnScreenshotTakenCallback callback,
    const std::vector<gfx::Rect>& bounds) {
#if BUILDFLAG(ENABLE_PDF)
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(
          lens_search_controller_->GetTabInterface()->GetContents());
  if (pdf_helper) {
    pdf_helper->GetMostVisiblePageIndex(base::BindOnce(
        &LensSearchContextualizationController::DidCaptureScreenshot,
        weak_ptr_factory_.GetWeakPtr(), std::move(chrome_render_frame),
        attempt_id, bitmap, bounds, std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  DidCaptureScreenshot(std::move(chrome_render_frame), attempt_id, bitmap,
                       bounds, std::move(callback),
                       /*pdf_current_page=*/std::nullopt);
}

void LensSearchContextualizationController::RecordInnerTextSize(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (!result) {
    return;
  }
  lens::RecordDocumentSizeBytes(lens::MimeType::kPlainText,
                                result->inner_text.size());
}

std::vector<lens::mojom::CenterRotatedBoxPtr>
LensSearchContextualizationController::ConvertSignificantRegionBoxes(
    const std::vector<gfx::Rect>& all_bounds) {
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes;
  int max_regions = lens::features::GetLensOverlayMaxSignificantRegions();
  if (max_regions == 0) {
    return significant_region_boxes;
  }
  content::RenderFrameHost* render_frame_host =
      lens_search_controller_->GetTabInterface()
          ->GetContents()
          ->GetPrimaryMainFrame();
  auto view_bounds = render_frame_host->GetView()->GetViewBounds();
  for (auto& image_bounds : all_bounds) {
    // Check the original area of the images against the minimum area.
    if (image_bounds.width() * image_bounds.height() >=
        lens::features::GetLensOverlaySignificantRegionMinArea()) {
      // We only have bounds for images in the main frame of the tab (i.e. not
      // in iframes), so view bounds are identical to tab bounds and can be
      // used for both parameters.
      significant_region_boxes.emplace_back(
          lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
              view_bounds, view_bounds, image_bounds));
    }
  }
  // If an image is outside the viewpoint, the box will have zero area.
  std::erase_if(significant_region_boxes, [](const auto& box) {
    return box->box.height() == 0 || box->box.width() == 0;
  });
  // Sort by descending area.
  std::sort(significant_region_boxes.begin(), significant_region_boxes.end(),
            [](const auto& box1, const auto& box2) {
              return box1->box.height() * box1->box.width() >
                     box2->box.height() * box2->box.width();
            });
  // Treat negative values of max_regions as no limit.
  if (max_regions > 0 && significant_region_boxes.size() >
                             static_cast<unsigned long>(max_regions)) {
    significant_region_boxes.resize(max_regions);
  }

  return significant_region_boxes;
}

float LensSearchContextualizationController::GetUiScaleFactor() {
  int device_scale_factor = lens_search_controller_->GetTabInterface()
                                ->GetContents()
                                ->GetRenderWidgetHostView()
                                ->GetDeviceScaleFactor();
  float page_scale_factor =
      zoom::ZoomController::FromWebContents(
          lens_search_controller_->GetTabInterface()->GetContents())
          ->GetZoomPercent() /
      100.0f;
  return device_scale_factor * page_scale_factor;
}

lens::LensOverlayQueryController*
LensSearchContextualizationController::GetQueryController() {
  auto* query_controller =
      lens_search_controller_->lens_overlay_query_controller();
  CHECK(query_controller);
  return query_controller;
}

lens::LensSearchboxController*
LensSearchContextualizationController::GetSearchboxController() {
  auto* searchbox_controller =
      lens_search_controller_->lens_searchbox_controller();
  CHECK(searchbox_controller);
  return searchbox_controller;
}

}  // namespace lens
