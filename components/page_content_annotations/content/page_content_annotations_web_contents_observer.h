// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/common/search_result_extractor_client_status.h"
#include "components/page_content_annotations/content/annotate_page_content_request.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {

class AnnotatedPageContentRequest;
class PageContentAnnotationsService;
class PageContentExtractionService;

// This class is used to dispatch page content to the
// PageContentAnnotationsService to be annotated.
class PageContentAnnotationsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          PageContentAnnotationsWebContentsObserver>,
      public PageContentAnnotationsService::PageContentAnnotationsObserver {
 public:
  ~PageContentAnnotationsWebContentsObserver() override;

  PageContentAnnotationsWebContentsObserver(
      const PageContentAnnotationsWebContentsObserver&) = delete;
  PageContentAnnotationsWebContentsObserver& operator=(
      const PageContentAnnotationsWebContentsObserver&) = delete;

  // Returns the content visibility score for this web contents. Will be nullopt
  // if not calculated yet.
  std::optional<float> content_visibility_score() {
    return content_visibility_score_;
  }

  PageContentAnnotationsWebContentsObserver(
      content::WebContents* web_contents,
      PageContentAnnotationsService& page_content_annotations_service,
      PageContentExtractionService* page_content_extraction_service,
      FetchPageContextCallback fetch_page_context_callback,
      GetTabIdCallback get_tab_id_callback);

 private:
  friend class content::WebContentsUserData<
      PageContentAnnotationsWebContentsObserver>;
  friend class PageContentAnnotationsWebContentsObserverTest;
  friend class PageContentExtractionService;

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DidStopLoading() override;
  void PrimaryPageChanged(content::Page& page) override;
  void OnFirstContentfulPaintInPrimaryMainFrame() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Invoked when related searches have been extracted for |visit|.
  void OnRelatedSearchesExtracted(
      const HistoryVisit& visit,
      continuous_search::SearchResultExtractorClientStatus status,
      continuous_search::mojom::CategoryResultsPtr results);

  // PageContentAnnotationsService::PageContentAnnotationsObserver:
  void OnPageContentAnnotated(
      const HistoryVisit& annotated_visit,
      const PageContentAnnotationsResult& result) override;

  // Returns the latest page content request to use. Could be null if extraction
  // is not enabled.
  AnnotatedPageContentRequest* GetAnnotatedPageContentRequest();

  std::unique_ptr<AnnotatedPageContentRequest> annotated_page_content_request_;

  raw_ref<PageContentAnnotationsService> page_content_annotations_service_;

  raw_ptr<PageContentExtractionService> page_content_extraction_service_;

  // The client of continuous_search::mojom::SearchResultExtractor
  // interface used for extracting data from the main frame of Google SRP
  // |web_contents|.
  continuous_search::SearchResultExtractorClient
      search_result_extractor_client_;

  const FetchPageContextCallback fetch_page_context_callback_;

  const GetTabIdCallback get_tab_id_callback_;

  std::optional<float> content_visibility_score_;

  base::WeakPtrFactory<PageContentAnnotationsWebContentsObserver>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTENT_ANNOTATIONS_WEB_CONTENTS_OBSERVER_H_
