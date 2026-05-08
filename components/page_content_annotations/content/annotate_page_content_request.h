// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/browser/page_settled_monitor.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
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
// TODO(b/490161242): Rename this class to reflect that it's the observer.
// TODO(b/487632737): Rename this class to reflect that it requests either:
// - `AnnotatedPageContent` for non-PDF pages.
// - Text for PDF pages.
class AnnotatedPageContentRequest
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AnnotatedPageContentRequest> {
 public:
  // LINT.IfChange(TriggerSource)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TriggerSource {
    kOnLoad = 0,
    kOnHidden = 1,
    kOnDemand = 2,
    kMaxValue = kOnDemand,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuidePageContentExtractionTriggerSource)

  static std::unique_ptr<AnnotatedPageContentRequest> Create(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

  AnnotatedPageContentRequest(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      blink::mojom::AIPageContentOptionsPtr request,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

  AnnotatedPageContentRequest(const AnnotatedPageContentRequest&) = delete;
  AnnotatedPageContentRequest& operator=(const AnnotatedPageContentRequest&) =
      delete;
  ~AnnotatedPageContentRequest() override;

  // Creates an instance for testing that is not attached as UserData.
  static std::unique_ptr<AnnotatedPageContentRequest> CreateForTesting(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);
  // Returns the cached APC for `page` and whether it is eligible for
  // server upload. Will return nullopt if not available or not supported (e.g.
  // for PDFs, the text extraction result is never cached).
  std::optional<ExtractedPageContentResult> GetCachedContentAndEligibility(
      bool log_metrics = true);

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
  // extraction to complete if there is one pending. If the extraction request
  // is cleared or reset (e.g. from a navigation or destruction), the callbacks
  // will resolve with std::nullopt. On-demand extraction is not supported for
  // PDFs and will also result in std::nullopt.
  void RefreshExtractedPageContentAndEligibilityForPage(
      GetExtractedPageContentAndEligibilityCallback callback);

 private:
  friend class content::WebContentsUserData<AnnotatedPageContentRequest>;
  friend class AnnotatePageContentRequestTest;

  AnnotatedPageContentRequest(
      content::WebContents* web_contents,
      PageContentExtractionService& page_content_extraction_service,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void OnFirstContentfulPaintInPrimaryMainFrame() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  void DidFinishNavigationWithPageSettledMonitor(
      content::NavigationHandle* navigation_handle);

  bool IsPdf() const;

  // Returns true if the async getter should wait for the extraction to
  // complete, or false if it should return immediately (with cached content or
  // nullopt).
  bool ShouldAsyncWaitForExtraction() const;
  void ResetForNewNavigation(bool is_same_document);

  // `on_hide` should be true iff this extraction is being triggered
  // specifically by the tab transitioning to hidden (as opposed to, say,
  // completing a page load).
  void MaybeScheduleExtraction(bool on_hide = false);

  // Returns the trigger source for an extraction, or nullopt if an extraction
  // should not be scheduled.
  [[nodiscard]] std::optional<TriggerSource> ShouldScheduleExtraction(
      bool on_hide) const;

  void OnExtractionTimerFired(TriggerSource trigger_source);
  void StartExtraction(TriggerSource trigger_source);
  void RequestAnnotatedPageContentSync(TriggerSource trigger_source);

  void OnPageContextFetched(TriggerSource trigger_source,
                            FetchPageContextResultCallbackArg result);

  void OnInnerTextReceived(
      base::TimeTicks start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  void ResolveAllCallbacksWith(
      const std::optional<ExtractedPageContentResult>& result);

  // Records metrics for the time from navigation to the page being ready for
  // extraction.
  void MaybeRecordReadyToExtractMetrics();

  bool IsPageReadyForExtraction() const;

#if BUILDFLAG(ENABLE_PDF)
  void RequestPdfPageCount();
  void RequestPdfText(TriggerSource trigger_source);

  // Invoked when pdf document is loaded, so that the metadata can be queried.
  void OnPdfDocumentLoadComplete();
#endif  // BUILDFLAG(ENABLE_PDF)

  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // Called when the page has settled, as determined by
  // `active_page_settled_monitor_`.
  void OnPageSettled();

  void SetPageSettledCallbackForTesting(base::OnceClosure callback);

  raw_ref<PageContentExtractionService> page_content_extraction_service_;
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;
  const blink::mojom::AIPageContentOptionsPtr options_;
  const bool include_inner_text_;
  const bool is_pdf_text_extraction_enabled_;
  // Whether the `PageSettledMonitor` should be used to determine when to
  // trigger the extraction. If false, the legacy mechanism (waiting for
  // load/FCP plus a fixed delay) will be used.
  const bool use_page_settled_monitor_;

  // LINT.IfChange(Lifecycle)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Tracks the state of the current extraction.
  enum class Lifecycle {
    // The state before any navigation has occurred. Extraction is not allowed
    // until the first navigation has occurred.
    kInitial = 0,

    // Indicates that a new navigation occurred and we may need to schedule an
    // extraction.
    kNavigated = 1,

    // An extraction has been scheduled and we are waiting for the delay timer
    // to fire.
    kScheduled = 2,

    // An extraction is in progress (e.g. after the delay timer fired) and we
    // are waiting for a response from the renderer.
    kRunning = 3,

    // An extraction has occurred and no others are currently scheduled.
    kExtracted = 4,

    kMaxValue = kExtracted,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OptimizationGuideOnDemandExtractionState2)
  Lifecycle lifecycle_ = Lifecycle::kInitial;

  // We don't record latency metrics for same-document navigations as we don't
  // get reliable load signals. So, these are only set for cross-document
  // navigations.
  std::optional<base::ElapsedTimer> stop_loading_timer_;
  std::optional<base::ElapsedTimer> extraction_timer_;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;
  bool waiting_for_page_settled_ = false;
  bool is_hidden_ = false;
  bool is_same_document_navigation_ = false;

  // The timer that starts when the current navigation committed. Reset to
  // `std::nullopt` once the "ready to extract" metric has been recorded for the
  // current navigation.
  std::optional<base::ElapsedTimer> navigation_commit_timer_;

  // `AnnotatedPageContentRequest` supports two different content requests:
  // - `AnnotatedPageContent` for non-PDF pages.
  // - Text for PDF pages.
  //
  // This cache only stores `AnnotatedPageContent`. PDF text is not cached here,
  // and as a result, on-demand extraction and other methods that rely on this
  // cache are not supported for PDFs.

  // TODO(b/503685696): Support on-demand extraction for PDF, which may require
  // adding PDF text extraction result to the cache.
  std::optional<ExtractedPageContentResult> cached_content_;

  std::vector<GetExtractedPageContentAndEligibilityCallback>
      on_demand_callbacks_;
  std::vector<GetExtractedPageContentAndEligibilityCallback>
      pending_content_callbacks_;
  std::vector<GetServerUploadEligibilityCallback>
      pending_eligibility_callbacks_;

  // Monitors created for pending navigations. Keyed by the NavigationHandle's
  // unique ID to handle interleaved navigations.
  base::flat_map<int64_t, std::unique_ptr<PageSettledMonitor>>
      pending_page_settled_monitors_;

  // The active monitor used to determine when the page has settled upon a
  // committed navigation. Only non-null if `use_page_settled_monitor_` is true.
  std::unique_ptr<PageSettledMonitor> active_page_settled_monitor_;

  FetchPageContextCallback fetch_page_context_callback_;
  GetTabIdCallback get_tab_id_callback_;

  base::OnceClosure page_settled_callback_for_testing_;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_H_
