// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/web_contents.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {
class PageContextEligibility;
}  // namespace optimization_guide

namespace page_content_annotations {

using GetExtractedPageContentAndEligibilityCallback =
    PageContentExtractionService::GetExtractedPageContentAndEligibilityCallback;
using GetServerUploadEligibilityCallback =
    PageContentExtractionService::GetServerUploadEligibilityCallback;

using FetchPageContextCallback =
    base::RepeatingCallback<void(content::WebContents&,
                                 const FetchPageContextOptions&,
                                 std::unique_ptr<FetchPageProgressListener>,
                                 FetchPageContextResultCallback)>;

using GetTabIdCallback =
    base::RepeatingCallback<std::optional<int64_t>(content::WebContents*)>;

// Class for deciding when a page is ready for getting page content, and
// extracts page content.
class AnnotatedPageContentRequest {
 public:
  static std::unique_ptr<AnnotatedPageContentRequest> Create(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

  AnnotatedPageContentRequest(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      blink::mojom::AIPageContentOptionsPtr options,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

  AnnotatedPageContentRequest(const AnnotatedPageContentRequest&) = delete;
  AnnotatedPageContentRequest& operator=(const AnnotatedPageContentRequest&) =
      delete;
  ~AnnotatedPageContentRequest();

  void PrimaryPageChanged();

  void DidFinishNavigation(content::NavigationHandle* navigation_handle);

  void DidStopLoading();

  void OnFirstContentfulPaintInPrimaryMainFrame();

  void OnVisibilityChanged(content::Visibility visibility);

  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available or not supported (e.g.
  // for PDFs).
  std::optional<ExtractedPageContentResult> GetCachedContentAndEligibility();

  // Returns whether the cached APC for `page` is eligible for server upload.
  // Will return nullopt if not available.
  std::optional<bool> GetServerUploadEligibility();

  // Asynchronous versions of the getter methods above.
  // These methods will resolve synchronously if the extraction is already
  // complete, or wait for the initial extraction to finish if there is one
  // pending. If the extraction request is cleared or reset (e.g. from a
  // navigation or destruction), the callbacks will resolve with std::nullopt.
  void GetCachedContentAndEligibilityAsync(
      GetExtractedPageContentAndEligibilityCallback callback);
  void GetServerUploadEligibilityAsync(
      GetServerUploadEligibilityCallback callback);

  // Extracts a new APC for `page` and computes its eligibility for server
  // upload, and caches the new result. It will wait for the initial
  // extraction to complete if there is one pending. For PDFs, it will return
  // the cached copy instead. If the extraction request is cleared or reset
  // (e.g. from a navigation or destruction), the callbacks will resolve with
  // std::nullopt. Extraction is not supported for PDFs and will also result in
  // nullopt.
  void RefreshExtractedPageContentAndEligibilityForPage(
      GetExtractedPageContentAndEligibilityCallback callback);

 private:
  bool IsPdf() const;

  // Returns true if the async getter should wait for the extraction to
  // complete, or false if it should return immediately (with cached content or
  // nullopt).
  bool ShouldAsyncWaitForExtraction() const;
  void ResetForNewNavigation();

  // `on_hide` should be true iff this extraction is being triggered
  // specifically by the tab transitioning to hidden (as opposed to, say,
  // completing a page load).
  void MaybeScheduleExtraction(bool on_hide = false);
  bool ShouldScheduleExtraction(bool on_hide) const;

  void OnExtractionTimerFired();
  void StartExtraction();
  void RequestAnnotatedPageContentSync();

  void OnPageContextFetched(FetchPageContextResultCallbackArg result);

  void OnInnerTextReceived(
      base::TimeTicks start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void ResolveAllCallbacksWith(
      const std::optional<ExtractedPageContentResult>& result);

#if BUILDFLAG(ENABLE_PDF)
  void RequestPdfPageCount();

  // Invoked when pdf document is loaded, so that the metadata can be queried.
  void OnPdfDocumentLoadComplete();
#endif  // BUILDFLAG(ENABLE_PDF)

  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  raw_ref<PageContentExtractionService> page_content_extraction_service_;
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;
  const raw_ptr<content::WebContents> web_contents_;
  const blink::mojom::AIPageContentOptionsPtr options_;
  const base::TimeDelta delay_;
  const bool include_inner_text_;

  // LINT.IfChange(Lifecycle)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Tracks the state of the current extraction.
  enum class Lifecycle {
    // Indicates that a new navigation occurred and we may need to schedule an
    // extraction.
    kNavigated = 0,

    // An extraction has been scheduled and we are waiting for the delay timer
    // to fire.
    kScheduled = 1,

    // An extraction is in progress (e.g. after the delay timer fired) and we
    // are waiting for a response from the renderer.
    kRunning = 2,

    // An extraction has occurred and no others are currently scheduled.
    kExtracted = 3,

    kMaxValue = kExtracted,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideOnDemandExtractionState)
  Lifecycle lifecycle_ = Lifecycle::kExtracted;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;
  bool is_hidden_ = false;

  std::optional<ExtractedPageContentResult> cached_content_;

  std::vector<GetExtractedPageContentAndEligibilityCallback>
      on_demand_callbacks_;
  std::vector<GetExtractedPageContentAndEligibilityCallback>
      pending_content_callbacks_;
  std::vector<GetServerUploadEligibilityCallback>
      pending_eligibility_callbacks_;

  FetchPageContextCallback fetch_page_context_callback_;
  GetTabIdCallback get_tab_id_callback_;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_
