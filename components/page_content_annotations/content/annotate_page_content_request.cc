// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/annotate_page_content_request.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/history/core/browser/features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "net/http/http_response_headers.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace page_content_annotations {

namespace {

#if BUILDFLAG(ENABLE_PDF)
void RecordPdfPageCountMetrics(
    ukm::SourceId source_id,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  if (status == pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed) {
    return;
  }
  ukm::builders::OptimizationGuide_AnnotatedPdfContent(source_id)
      .SetPdfPageCount(ukm::GetExponentialBucketMinForCounts1000(page_count))
      .Record(ukm::UkmRecorder::Get());
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool ContainsAnnotatedPageContent(const FetchPageContextResult& result) {
  return result.annotated_page_content_result.has_value();
}

bool ContainsPDFText(const FetchPageContextResult& result) {
  return result.pdf_result.has_value() &&
         std::holds_alternative<std::string>(result.pdf_result->data);
}

std::optional<PageContent> TakePageContent(FetchPageContextResult result) {
  if (base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentPDFTextExtraction)) {
    // Handling PDF text from the result.
    if (ContainsPDFText(result)) {
      // The `FetchPageContextResult` cannot contain both AnnotatedPageContent
      // and PDF text at the same time.
      CHECK(!ContainsAnnotatedPageContent(result));

      return base::MakeRefCounted<RefCountedPDFText>(
          std::get<std::string>(std::move(result.pdf_result->data)));
    }
  }

  // Handling APC from the result.
  if (ContainsAnnotatedPageContent(result)) {
    // The `FetchPageContextResult` cannot contain both AnnotatedPageContent
    // and PDF text at the same time.
    CHECK(!ContainsPDFText(result));
    base::expected<PageContentResultWithEndTime, std::string>
        annotated_page_content_result =
            std::move(result.annotated_page_content_result);

    return base::MakeRefCounted<RefCountedAnnotatedPageContent>(
        std::move(annotated_page_content_result->proto));
  }

  // Neither APC nor PDF text is available.
  return std::nullopt;
}

std::optional<ExtractedPageContentResult>
RecordAndReturnOnDemandExtractionResult(
    base::ElapsedTimer timer,
    std::optional<ExtractedPageContentResult> result) {
  base::UmaHistogramTimes(
      "OptimizationGuide.PageContentExtraction.OnDemand.Latency",
      timer.Elapsed());
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentExtraction.OnDemand.Success",
      result.has_value());
  return result;
}

blink::mojom::AIPageContentOptionsPtr CreateOptions() {
  auto options = blink::mojom::AIPageContentOptions::New();
  options->mode =
      (page_content_annotations::features::AnnotatedPageContentMode() ==
       "actionable")
          ? blink::mojom::AIPageContentMode::kActionableElements
          : blink::mojom::AIPageContentMode::kDefault;
  options->on_critical_path = page_content_annotations::features::
      IsAnnotatedPageContentOnCriticalPath();

  if (page_content_annotations::features::
          ShouldAnnotatedPageContentExcludeAdRelated()) {
    options->non_salient_content_config =
        blink::mojom::NonSalientContentConfig::New();
    options->non_salient_content_config->exclude_ad_related = true;
  }

  return options;
}

}  // namespace

AnnotatedPageContentRequest::AnnotatedPageContentRequest(
    content::WebContents* web_contents,
    PageContentExtractionService& page_content_extraction_service,
    FetchPageContextCallback fetch_page_context_callback,
    GetTabIdCallback get_tab_id_callback)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AnnotatedPageContentRequest>(*web_contents),
      page_content_extraction_service_(page_content_extraction_service),
      options_(CreateOptions()),
      include_inner_text_(
          features::ShouldAnnotatedPageContentStudyIncludeInnerText()),
      is_pdf_text_extraction_enabled_(base::FeatureList::IsEnabled(
          features::kAnnotatedPageContentPDFTextExtraction)),
      fetch_page_context_callback_(std::move(fetch_page_context_callback)),
      get_tab_id_callback_(std::move(get_tab_id_callback)) {
  // Post to a background thread to avoid blocking the set up of the overlay.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(
          &AnnotatedPageContentRequest::OnPageContextEligibilityAPILoaded,
          weak_factory_.GetWeakPtr()));
}

AnnotatedPageContentRequest::~AnnotatedPageContentRequest() {
  ResolveAllCallbacksWith(std::nullopt);
}

// static
std::unique_ptr<AnnotatedPageContentRequest>
AnnotatedPageContentRequest::CreateForTesting(  // IN-TEST
    content::WebContents* web_contents,
    PageContentExtractionService& page_content_extraction_service,
    FetchPageContextCallback fetch_page_context_callback,
    GetTabIdCallback get_tab_id_callback) {
  return base::WrapUnique(new AnnotatedPageContentRequest(
      web_contents, page_content_extraction_service,
      std::move(fetch_page_context_callback), std::move(get_tab_id_callback)));
}

void AnnotatedPageContentRequest::PrimaryPageChanged(content::Page& page) {
  ResetForNewNavigation(/*is_same_document=*/false);
}

void AnnotatedPageContentRequest::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Cross-document navigations are handled in PrimaryPageChanged.
  if (!navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // This is a heuristic to tradeoff how frequently the content is updated and
  // ensuring we have coverage for single-page-apps in the data. If the
  // navigation will appear in the browser history, it's likely a significant
  // change in page state.
  if (!navigation_handle->ShouldUpdateHistory()) {
    return;
  }

  if (base::FeatureList::IsEnabled(history::kVisitedLinksOn404)) {
    // With the flag enabled, navigations with a 404 status code will be
    // eligible for History. We want to ignore 404s. At this point, we should
    // only be looking at committed same-document navigations. Same-document
    // navigations have no network request and therefore no response code, so we
    // should look at the response code for the request that brought us to the
    // current document instead of the `NavigationHandle`.
    const auto* document_response_head =
        navigation_handle->GetRenderFrameHost()->GetLastResponseHead();
    if (!document_response_head || !document_response_head->headers) {
      return;
    }
    const int status_code = document_response_head->headers->response_code();
    if (status_code == 404) {
      return;
    }
  }

  ResetForNewNavigation(/*is_same_document=*/true);
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::DidStopLoading() {
  // Ensure that the main frame's Document has finished loading.
  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }

  // Once the main Document has fired the `load` event, wait for all subframes
  // currently in the FrameTree to also finish loading.
  if (web_contents()->IsLoading()) {
    return;
  }

  if (IsPdf() ||
      web_contents()->GetVisibility() == content::Visibility::HIDDEN ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPageContentAnnotationsSkipFCPWaitForTesting)) {
    // Pdfs and hidden tabs don't provide a reliable FirstContentfulPaint
    // signal, so skip waiting for it for these Documents.
    waiting_for_fcp_ = false;
  }

  // TODO(b/490161242): Investigate if we should return early here if we are not
  // waiting for load to avoid duplicate extractions for same-document
  // navigations.
  if (waiting_for_load_) {
    // Only set the timer for cross-document navigations.
    stop_loading_timer_ = base::ElapsedTimer();
  }

  waiting_for_load_ = false;
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::OnFirstContentfulPaintInPrimaryMainFrame() {
  waiting_for_fcp_ = false;
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::ResetForNewNavigation(bool is_same_document) {
  lifecycle_ = Lifecycle::kNavigated;

  // We don't have reliable load and FCP signals for same-document navigations.
  // So we assume the content is ready as soon as the navigation commits.
  waiting_for_fcp_ = !is_same_document;
  waiting_for_load_ = !is_same_document;

  cached_content_ = std::nullopt;

  ResolveAllCallbacksWith(std::nullopt);

  // Drop pending extraction request for the previous page, if any.
  weak_factory_.InvalidateWeakPtrs();

  stop_loading_timer_ = std::nullopt;
  extraction_timer_ = std::nullopt;

  page_content_extraction_service_->OnNewNavigation(
      get_tab_id_callback_.Run(web_contents()), web_contents(),
      is_same_document);
}

void AnnotatedPageContentRequest::MaybeScheduleExtraction(bool on_hide) {
  std::optional<TriggerSource> trigger_source =
      ShouldScheduleExtraction(on_hide);
  if (!trigger_source) {
    return;
  }

  lifecycle_ = Lifecycle::kScheduled;

  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AnnotatedPageContentRequest::OnExtractionTimerFired,
                     weak_factory_.GetWeakPtr(), *trigger_source),
      features::GetAnnotatedPageContentCaptureDelay());
}

void AnnotatedPageContentRequest::OnExtractionTimerFired(
    TriggerSource trigger_source) {
  // If there was a navigation in between the delay, skip extraction.
  if (lifecycle_ != Lifecycle::kScheduled) {
    return;
  }

  StartExtraction(trigger_source);
}

void AnnotatedPageContentRequest::StartExtraction(
    TriggerSource trigger_source) {
  lifecycle_ = Lifecycle::kRunning;

  // We don't record metrics for same-document navigations.
  if (stop_loading_timer_) {
    extraction_timer_ = base::ElapsedTimer();

    if (trigger_source == TriggerSource::kOnLoad) {
      base::UmaHistogramTimes(
          "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
          "StabilityLatency",
          stop_loading_timer_->Elapsed());
    }
  }

  if (IsPdf()) {
#if BUILDFLAG(ENABLE_PDF)
    if (is_pdf_text_extraction_enabled_) {
      RequestPdfText(trigger_source);
    } else {
      RequestPdfPageCount();
    }
#endif  // BUILDFLAG(ENABLE_PDF)
  } else {
    RequestAnnotatedPageContentSync(trigger_source);
  }
}

void AnnotatedPageContentRequest::RequestAnnotatedPageContentSync(
    TriggerSource trigger_source) {
  TRACE_EVENT0("browser",
               "AnnotatedPageContentRequest::RequestAnnotatedPageContentSync");

  // Note: This is not fetching pdfs since we do not want to cache pdfs in disk
  // in PageContentCache.
  FetchPageContextOptions options;
  options.annotated_page_content_options = options_->Clone();
  if (features::kPageContentCacheEnableScreenshot.Get()) {
    ScreenshotOptions::ScreenshotCollectionOptions
        screenshot_collection_options;
    screenshot_collection_options.screenshot_image_format =
        ScreenshotOptions::ScreenshotImageFormat::kPng;
    options.screenshot_options =
        ScreenshotOptions::ViewportOnly(std::nullopt, std::nullopt);
  }
  fetch_page_context_callback_.Run(
      *web_contents(), options, /*progress_listener=*/nullptr,
      base::BindOnce(&AnnotatedPageContentRequest::OnPageContextFetched,
                     weak_factory_.GetWeakPtr(), trigger_source));

  if (include_inner_text_) {
    content_extraction::GetInnerText(
        *web_contents()->GetPrimaryMainFrame(), std::nullopt,
        base::BindOnce(&AnnotatedPageContentRequest::OnInnerTextReceived,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  }
}

std::optional<AnnotatedPageContentRequest::TriggerSource>
AnnotatedPageContentRequest::ShouldScheduleExtraction(bool on_hide) const {
  if (!page_content_extraction_service_->ShouldEnablePageContentExtraction()) {
    return std::nullopt;
  }

  // If the page is not loaded, the extraction would not work.
  if (waiting_for_fcp_ || waiting_for_load_) {
    return std::nullopt;
  }

  if (lifecycle_ == Lifecycle::kInitial ||
      lifecycle_ == Lifecycle::kScheduled ||
      lifecycle_ == Lifecycle::kRunning) {
    // Until the initial navigation completes, extraction is disallowed.
    // Otherwise, an extraction is already scheduled or running, no need to
    // duplicate.
    return std::nullopt;
  }

  auto triggering_mode = features::GetPageContentExtractionTriggeringMode();
  bool trigger_on_hide =
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnHidden ||
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoadAndHidden;

  // PDF extraction skips the on-hidden trigger to avoid redundant extraction of
  // static content.
  if (trigger_on_hide && !IsPdf()) {
    // We trigger extraction any time the page transitions to hidden, or if the
    // page finished loading while already in the background.
    bool newly_hidden =
        on_hide || (lifecycle_ == Lifecycle::kNavigated && is_hidden_);
    if (newly_hidden) {
      CHECK(is_hidden_);
      return TriggerSource::kOnHidden;
    }
  }

  if (lifecycle_ != Lifecycle::kNavigated) {
    return std::nullopt;
  }

  bool trigger_on_load =
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoad ||
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoadAndHidden;

  if (trigger_on_load || !on_demand_callbacks_.empty()) {
    // TODO(b/490161242): Investigate why this check can fail and then consider
    // re-enabling it.
    // CHECK(!on_hide);
    if (!on_demand_callbacks_.empty()) {
      return TriggerSource::kOnDemand;
    }
    return TriggerSource::kOnLoad;
  }

  return std::nullopt;
}

void AnnotatedPageContentRequest::OnPageContextFetched(
    TriggerSource trigger_source,
    FetchPageContextResultCallbackArg result) {
  lifecycle_ = Lifecycle::kExtracted;

  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentExtraction.TriggerSource", trigger_source);

  CHECK_EQ(stop_loading_timer_.has_value(), extraction_timer_.has_value());
  if (trigger_source == TriggerSource::kOnLoad && extraction_timer_) {
    base::UmaHistogramTimes(
        "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
        "ExtractionLatency",
        extraction_timer_->Elapsed());
    base::UmaHistogramTimes(
        "OptimizationGuide.PageContentExtraction.AutomaticOnLoad."
        "OverallLatency",
        stop_loading_timer_->Elapsed());
  }

  // The page context result is null.
  if (!result.has_value() || !result.value()) {
    ResolveAllCallbacksWith(std::nullopt);
    return;
  }

  base::Time extraction_time = base::Time::Now();
  FetchPageContextResult& page_content_result = *result.value();

  // Retrieve screenshot data. Screenshot data must be retrieved before
  // `TakePageContent` to avoid use-after-move.
  std::vector<uint8_t> screenshot_data;
  if (page_content_result.screenshot_result.has_value()) {
    screenshot_data = std::move(
        page_content_result.screenshot_result.value().screenshot_data);
  }

  // Calculate server upload eligibility. Note this applies to the result if and
  // only if it holds an APC. Eligibility must be calculated before
  // `TakePageContent` to avoid use-after-move.
  bool is_eligible_for_server_upload = false;
  if (ContainsAnnotatedPageContent(page_content_result)) {
    GURL url = web_contents()->GetLastCommittedURL();
    is_eligible_for_server_upload =
        !page_context_eligibility_ ||
        optimization_guide::IsPageContextEligible(
            url.GetHost(), url.GetPath(),
            optimization_guide::GetFrameMetadataFromPageContent(
                page_content_result.annotated_page_content_result.value()),
            page_context_eligibility_);
  }

  // Take either the APC or PDF text from the result.
  std::optional<PageContent> page_content =
      TakePageContent(std::move(page_content_result));

  // Cannot find APC or PDF text in page context result.
  if (!page_content) {
    ResolveAllCallbacksWith(/*result=*/std::nullopt);
    return;
  }

  // Notify page content extraction service with `page_content`, which holds
  // either the APC for a non-PDF page; or the PDF text for a PDF page.
  page_content_extraction_service_->OnPageContentExtracted(
      web_contents()->GetPrimaryPage(), page_content.value(), screenshot_data,
      get_tab_id_callback_.Run(web_contents()));

  if (IsPDFTextPtr(page_content.value())) {
    // Note: Unlike APC result, PDF text result is not stored to the
    // `cached_content_` below, which is used for supporting on-demand APC
    // fetching.
    // TODO(b/487632737): Investigate the support for on-demand PDF text
    // extraction, which may require storing the result to `cached_content_`.
    ResolveAllCallbacksWith(/*result=*/std::nullopt);
    return;
  }

  // Move APC into the cache.
  cached_content_ =
      ExtractedPageContentResult(GetAnnotatedPageContentPtrFromPageContent(
                                     std::move(page_content.value())),
                                 extraction_time, is_eligible_for_server_upload,
                                 std::move(screenshot_data));

  ResolveAllCallbacksWith(cached_content_);
}

void AnnotatedPageContentRequest::OnInnerTextReceived(
    base::TimeTicks start_time,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (!result) {
    return;
  }
  UMA_HISTOGRAM_TIMES("OptimizationGuide.InnerText.TotalLatency",
                      base::TimeTicks::Now() - start_time);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OptimizationGuide.InnerText.TotalSize2",
                              result->inner_text.length() / 1024, 10, 5000, 50);
}

#if BUILDFLAG(ENABLE_PDF)
void AnnotatedPageContentRequest::RequestPdfPageCount() {
  CHECK(IsPdf());
  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
  if (pdf_helper) {
    pdf_helper->RegisterForDocumentLoadComplete(
        base::BindOnce(&AnnotatedPageContentRequest::OnPdfDocumentLoadComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void AnnotatedPageContentRequest::RequestPdfText(TriggerSource trigger_source) {
  TRACE_EVENT0("browser", "AnnotatedPageContentRequest::RequestPdfText");
  CHECK(IsPdf());

  FetchPageContextOptions options;
  options.pdf_options.emplace(PdfOptions::Format::kText,
                              features::MaxPDFTextExtractionByteSize());

  // PDF text extraction is triggered with a default 3-second delay after the
  // page stops loading (see `kAnnotatedPageContentCaptureDelay`).
  //
  // For most PDFs, the first page renders within this delay (see UMA
  // histogram `PDF.FirstPaintTime`). If it is not rendered, extraction may
  // return an empty string.
  //
  // TODO(b/422120832): This fixed delay will be replaced by
  // `PageSettledMonitor`, the general page stability detection mechanism also
  // used by Actor. This implementation may require changes when that happens.
  fetch_page_context_callback_.Run(
      *web_contents(), options, /*progress_listener=*/nullptr,
      base::BindOnce(&AnnotatedPageContentRequest::OnPageContextFetched,
                     weak_factory_.GetWeakPtr(), trigger_source));
}

void AnnotatedPageContentRequest::OnPdfDocumentLoadComplete() {
  CHECK(IsPdf());
  lifecycle_ = Lifecycle::kExtracted;

  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents());
  if (pdf_helper) {
    // Fetch zero PDF bytes to just receive the total page count.
    pdf_helper->GetPdfBytes(
        /*size_limit=*/0,
        base::BindOnce(
            &RecordPdfPageCountMetrics,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()));
  }

  // Requests for PDFs are synchronously rejected in
  // RefreshExtractedPageContentAndEligibilityForPage. Therefore, they never get
  // added to the on_demand_callbacks_ queue, so it will always be empty here.
  CHECK(on_demand_callbacks_.empty());

  ResolveAllCallbacksWith(std::nullopt);
}
#endif  // BUILDFLAG(ENABLE_PDF)

void AnnotatedPageContentRequest::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
}

std::optional<ExtractedPageContentResult>
AnnotatedPageContentRequest::GetCachedContentAndEligibility(bool log_metrics) {
  if (log_metrics) {
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentExtraction.IsCacheHit",
        cached_content_.has_value());
  }
  return cached_content_;
}

std::optional<bool> AnnotatedPageContentRequest::GetServerUploadEligibility() {
  return cached_content_ ? std::make_optional(
                               cached_content_->is_eligible_for_server_upload)
                         : std::nullopt;
}

bool AnnotatedPageContentRequest::IsPdf() const {
  return web_contents()->GetContentsMimeType() == pdf::kPDFMimeType;
}

bool AnnotatedPageContentRequest::ShouldAsyncWaitForExtraction() const {
  if (!page_content_extraction_service_->ShouldEnablePageContentExtraction()) {
    return false;
  }

  if (IsPdf()) {
    CHECK(!cached_content_.has_value());
    return false;
  }

  switch (lifecycle_) {
    case Lifecycle::kInitial:
      return false;
    case Lifecycle::kNavigated: {
      bool is_on_hidden_mode =
          features::GetPageContentExtractionTriggeringMode() ==
          features::PageContentExtractionTriggeringMode::kOnHidden;
      if (is_on_hidden_mode && !is_hidden_) {
        // In 'on hidden' mode, extraction is only triggered when the page
        // becomes hidden. If it is currently visible, no extraction is
        // scheduled yet, so we return false to avoid waiting indefinitely.
        CHECK(!cached_content_.has_value());
        return false;
      }
      return true;
    }
    case Lifecycle::kScheduled:
    case Lifecycle::kRunning:
      return true;
    case Lifecycle::kExtracted:
      return false;
  }
}

void AnnotatedPageContentRequest::GetCachedContentAndEligibilityAsync(
    GetExtractedPageContentAndEligibilityCallback callback) {
  if (ShouldAsyncWaitForExtraction()) {
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentExtraction.IsCacheHit", false);
    pending_content_callbacks_.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(GetCachedContentAndEligibility());
}

void AnnotatedPageContentRequest::GetServerUploadEligibilityAsync(
    GetServerUploadEligibilityCallback callback) {
  if (ShouldAsyncWaitForExtraction()) {
    pending_eligibility_callbacks_.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(GetServerUploadEligibility());
}

void AnnotatedPageContentRequest::
    RefreshExtractedPageContentAndEligibilityForPage(
        GetExtractedPageContentAndEligibilityCallback callback) {
  if (!page_content_extraction_service_->ShouldEnablePageContentExtraction()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentExtraction.OnDemand.IsPDF", IsPdf());

  // PDF text is extracted when 'AnnotatedPageContentPDFTextExtraction' feature
  // is enabled. However, the result is never cached.
  if (IsPdf()) {
    CHECK(!cached_content_.has_value());
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentExtraction.OnDemand.StateAtRequest2",
      lifecycle_);

  auto wrapped_callback =
      base::BindOnce(&RecordAndReturnOnDemandExtractionResult,
                     base::ElapsedTimer())
          .Then(std::move(callback));
  on_demand_callbacks_.push_back(std::move(wrapped_callback));

  // This on-demand request must coordinate with the automatic extraction.
  if (on_demand_callbacks_.size() == 1) {
    switch (lifecycle_) {
      case Lifecycle::kNavigated:
        // The initial extraction has not been scheduled. Force it to be
        // scheduled if the page is ready. This is a no-op unless the
        // triggering mode is "on hidden" only.
        // TODO(b/490161242): Consider shortening the delay based on how long
        // ago the page navigated.
        MaybeScheduleExtraction();
        break;
      case Lifecycle::kInitial:
      case Lifecycle::kScheduled:
      case Lifecycle::kRunning:
        // Initial navigation not complete, or already scheduled or running.
        // Wait for it.
        break;
      case Lifecycle::kExtracted:
        // The previous extraction is complete. Start a new one immediately.
        StartExtraction(TriggerSource::kOnDemand);
        break;
    }
  }
}

void AnnotatedPageContentRequest::ResolveAllCallbacksWith(
    const std::optional<ExtractedPageContentResult>& result) {
  if (!on_demand_callbacks_.empty() || !pending_content_callbacks_.empty() ||
      !pending_eligibility_callbacks_.empty()) {
    base::UmaHistogramCounts100(
        "OptimizationGuide.PageContentExtraction.OnDemand."
        "PendingCallbacksBatched",
        on_demand_callbacks_.size());
    base::UmaHistogramCounts100(
        "OptimizationGuide.PageContentExtraction.Async."
        "PendingContentCallbacksBatched",
        pending_content_callbacks_.size());
    base::UmaHistogramCounts100(
        "OptimizationGuide.PageContentExtraction.Async."
        "PendingEligibilityCallbacksBatched",
        pending_eligibility_callbacks_.size());
  }

  auto on_demand_callbacks = std::exchange(on_demand_callbacks_, {});
  auto pending_content_callbacks =
      std::exchange(pending_content_callbacks_, {});
  auto pending_eligibility_callbacks =
      std::exchange(pending_eligibility_callbacks_, {});

  // TODO(b/490161242): Consider wrapping the screenshot data (or the whole
  // ExtractedPageContentResult) in a scoped_refptr to avoid copying for each of
  // the callbacks.
  for (auto& callback : on_demand_callbacks) {
    std::move(callback).Run(result);
  }

  for (auto& callback : pending_content_callbacks) {
    std::move(callback).Run(result);
  }

  std::optional<bool> server_eligibility =
      result ? std::make_optional(result->is_eligible_for_server_upload)
             : std::nullopt;
  for (auto& callback : pending_eligibility_callbacks) {
    std::move(callback).Run(server_eligibility);
  }
}

void AnnotatedPageContentRequest::OnVisibilityChanged(
    content::Visibility visibility) {
  bool was_hidden = is_hidden_;
  is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (is_hidden_ == was_hidden) {
    return;
  }

  page_content_extraction_service_->OnVisibilityChanged(
      get_tab_id_callback_.Run(web_contents()), web_contents(), visibility);

  if (is_hidden_) {
    MaybeScheduleExtraction(/*on_hide=*/true);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AnnotatedPageContentRequest);

}  // namespace page_content_annotations
