// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/lens/lens_features.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace lens {

DEFINE_USER_DATA(TabContextualizationController);

TabContextualizationController::TabContextualizationController(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      tab_(tab) {
  tab_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&TabContextualizationController::WillDiscardContents,
                          weak_ptr_factory_.GetWeakPtr()));
  screenshot_task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  CreatePageContextEligibilityAPI();
}

TabContextualizationController::~TabContextualizationController() = default;

void TabContextualizationController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

TabContextualizationController* TabContextualizationController::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void TabContextualizationController::CreatePageContextEligibilityAPI() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(
          &TabContextualizationController::OnPageContextEligibilityAPILoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

void TabContextualizationController::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
}

void TabContextualizationController::PrimaryPageChanged(content::Page& page) {
  is_page_context_eligible_ = false;
}

void TabContextualizationController::OnEligibilityChecked(
    bool is_page_context_eligible,
    optimization_guide::AIPageContentResultOrError apc) {
  is_page_context_eligible_ = is_page_context_eligible;
}

void TabContextualizationController::UpdatePageContextEligibility(
    GetApcResultCallback callback) {
  auto* render_frame_host = tab_->GetContents()->GetPrimaryMainFrame();
  if (!render_frame_host) {
    return;
  }

  GetAnnotatedPageContent(base::BindOnce(
      &TabContextualizationController::OnAnnotatedPageContentReceived,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabContextualizationController::GetAnnotatedPageContent(
    GetAnnotatedPageContentCallback callback) {
  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  ai_page_content_options->max_meta_elements = 20;
  ai_page_content_options->include_same_site_only = true;
  optimization_guide::GetAIPageContent(tab_->GetContents(),
                                       std::move(ai_page_content_options),
                                       std::move(callback));
}

void TabContextualizationController::OnAnnotatedPageContentReceived(
    GetApcResultCallback callback,
    optimization_guide::AIPageContentResultOrError result) {
  // The tab URL is used to check if the page is context eligible.
  const auto& tab_url = tab_->GetContents()->GetLastCommittedURL();

  std::vector<optimization_guide::FrameMetadata> frame_metadata_structs;
  if (result.has_value()) {
    // Convert the page metadata to a C struct defined in the
    // `optimization_guide` component so it can be passed to the shared library.
    frame_metadata_structs =
        optimization_guide::GetFrameMetadataFromPageContent(result.value());
  }

  std::move(callback).Run(
      optimization_guide::IsPageContextEligible(
          tab_url.GetHost(), tab_url.GetPath(),
          std::move(frame_metadata_structs), page_context_eligibility_),
      std::move(result));
}

void TabContextualizationController::
    OnApcAndEligibilityReceivedForGetPageContext(
        GetPageContextCallback callback,
        std::unique_ptr<lens::ContextualInputData> data,
        bool page_context_eligible,
        optimization_guide::AIPageContentResultOrError result) {
  data->is_page_context_eligible = page_context_eligible;
  data->primary_content_type = lens::MimeType::kAnnotatedPageContent;
  data->context_input = std::vector<lens::ContextualInput>();
  if (!page_context_eligible || !result.has_value()) {
    // Early return if the page is not context eligible.
    std::move(callback).Run(std::move(data));
    return;
  }

  std::string serialized_apc;
  result->proto.SerializeToString(&serialized_apc);
  data->context_input->emplace_back(
      std::vector<uint8_t>(serialized_apc.begin(), serialized_apc.end()),
      lens::MimeType::kAnnotatedPageContent);

  // TODO(crbug.com/443743308): Parallelize the screenshot capture with the
  // webpage bytes fetch.
  CaptureScreenshot(
      /*image_options=*/std::nullopt,
      base::BindOnce(&TabContextualizationController::
                         AddScreenshotToContextDataAndContinue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(data)));
}

bool TabContextualizationController::GetInitialPageContextEligibility() {
  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* page_content_extraction_service = page_content_annotations::
      PageContentExtractionServiceFactory::GetForProfile(profile);

  if (!page_content_extraction_service) {
    return false;
  }

  std::optional<page_content_annotations::ExtractedPageContentResult>
      extracted_page_content_result =
          page_content_extraction_service
              ->GetExtractedPageContentAndEligibilityForPage(
                  web_contents->GetPrimaryPage());

  return !extracted_page_content_result ||
         extracted_page_content_result->is_eligible_for_server_upload;
}

bool TabContextualizationController::GetCurrentPageContextEligibility() {
  return is_page_context_eligible_;
}

void TabContextualizationController::GetPageContext(
    GetPageContextCallback callback) {
  auto contextual_input_data = std::make_unique<lens::ContextualInputData>();

  // TODO(crbug.com/439595898): Get contextual input bytes using APC. Also,
  // populate the mime type, tab eligibility, and pdf current page.
  contextual_input_data->context_input = {};

  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  contextual_input_data->tab_session_id =
      sessions::SessionTabHelper::IdForTab(web_contents);
  contextual_input_data->page_url = web_contents->GetLastCommittedURL();
  contextual_input_data->page_title =
      base::UTF16ToUTF8(web_contents->GetTitle());

#if BUILDFLAG(ENABLE_PDF)
  // Capture the PDF bytes if the PDF helper exists.
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
  if (pdf_helper) {
    // Fetch the PDF bytes then run the callback.
    pdf_helper->GetPdfBytes(
        /*size_limit=*/lens::features::GetLensOverlayFileUploadLimitBytes(),
        base::BindOnce(&TabContextualizationController::OnPdfBytesReceived,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(contextual_input_data), std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  // If the page is not a PDF, get the annotated page content.
  GetAnnotatedPageContent(base::BindOnce(
      &TabContextualizationController::OnAnnotatedPageContentReceived,
      weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&TabContextualizationController::
                         OnApcAndEligibilityReceivedForGetPageContext,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(contextual_input_data))));
}

#if BUILDFLAG(ENABLE_PDF)
void TabContextualizationController::OnPdfBytesReceived(
    std::unique_ptr<lens::ContextualInputData> data,
    GetPageContextCallback callback,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    std::move(callback).Run(nullptr);
    return;
  }

  data->primary_content_type = lens::MimeType::kPdf;
  data->context_input = std::vector<lens::ContextualInput>();
  // TODO(crbug.com/370530197): Show user error message if status is not
  // success.
  if (status != pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess ||
      page_count == 0) {
    std::move(callback).Run(std::move(data));
    return;
  }

  base::span<const uint8_t> file_data_span = base::span(bytes);
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), lens::MimeType::kPdf));

  // Get the most visible page index if the PDF helper exists.
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
  if (pdf_helper) {
    // TODO(crbug.com/443743308): Parallelize the PDF page index fetch with the
    // PDF bytes fetch.
    pdf_helper->GetMostVisiblePageIndex(base::BindOnce(
        &TabContextualizationController::OnPdfPageIndexReceived,
        weak_ptr_factory_.GetWeakPtr(), std::move(data), std::move(callback)));
    return;
  }

  // If the PDF helper no longer exists, set nullopt for the PDF page index.
  OnPdfPageIndexReceived(std::move(data), std::move(callback),
                         /*page_index=*/std::nullopt);
}

void TabContextualizationController::OnPdfPageIndexReceived(
    std::unique_ptr<lens::ContextualInputData> data,
    GetPageContextCallback callback,
    std::optional<uint32_t> page_index) {
  if (page_index.has_value()) {
    data->pdf_current_page = page_index.value();
  }

  // TODO(crbug.com/443743308): Parallelize the screenshot capture with the
  // PDF page index fetch.
  CaptureScreenshot(
      /*image_options=*/std::nullopt,
      base::BindOnce(&TabContextualizationController::
                         AddScreenshotToContextDataAndContinue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(data)));
}
#endif  // BUILDFLAG(ENABLE_PDF)

void TabContextualizationController::CaptureScreenshot(
    std::optional<lens::ImageEncodingOptions> image_options,
    CaptureScreenshotCallback callback) {
  content::WebContents* web_contents = tab_->GetContents();
  if (!web_contents) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  content::RenderWidgetHostView* view = web_contents->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  if (!view || !view->IsSurfaceAvailableForCopy()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  base::ScopedClosureRunner decrement_capturer_count_runner =
      web_contents->IncrementCapturerCount(
          /*capture_size=*/gfx::Size(),
          /*stay_hidden=*/true,
          /*stay_awake=*/false,
          /*is_activity=*/false);

  auto callback_wrapper = base::BindOnce(
      &TabContextualizationController::DownscaleScreenshotAndContinue,
      weak_ptr_factory_.GetWeakPtr(),
      std::move(decrement_capturer_count_runner), std::move(image_options),
      std::move(callback));

  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(callback_wrapper)));
}

void TabContextualizationController::DownscaleScreenshotAndContinue(
    base::ScopedClosureRunner decrement_capturer_count_runner,
    std::optional<lens::ImageEncodingOptions> image_options,
    CaptureScreenshotCallback callback,
    const viz::CopyOutputBitmapWithMetadata& result) {
  // `DecrementCapturerCount()` is called when `decrement_capturer_count_runner`
  // goes out of scope.
  const SkBitmap& screenshot = result.bitmap;

  if (screenshot.drawsNothing() || !image_options.has_value()) {
    std::move(callback).Run(screenshot);
    return;
  }

  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();

  // Downscaling is done on a background thread to avoid blocking the main
  // thread.
  screenshot_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&lens::DownscaleBitmap, screenshot, ref_counted_logs,
                     image_options.value()),
      std::move(callback));
}

void TabContextualizationController::AddScreenshotToContextDataAndContinue(
    GetPageContextCallback callback,
    std::unique_ptr<lens::ContextualInputData> data,
    const SkBitmap& screenshot) {
  if (!screenshot.drawsNothing()) {
    data->viewport_screenshot = screenshot;
  }

  std::move(callback).Run(std::move(data));
}

}  // namespace lens
