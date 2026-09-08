// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_content_annotations_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

namespace page_content_annotations {

namespace {

base::Time GetTimestampFromWebContents(content::WebContents* web_contents) {
  return web_contents->GetController().GetLastCommittedEntry()->GetTimestamp();
}

}  // namespace

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents,
        PageContentAnnotationsService& page_content_annotations_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageContentAnnotationsWebContentsObserver>(
          *web_contents),
      page_content_annotations_service_(page_content_annotations_service) {
  page_content_annotations_service_->AddObserver(
      AnnotationType::kContentVisibility, this);
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() {
  page_content_annotations_service_->RemoveObserver(
      AnnotationType::kContentVisibility, this);
}

void PageContentAnnotationsWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!features::ShouldExtractRelatedSearches()) {
    return;
  }
  if (!google_util::IsGoogleSearchUrl(web_contents()->GetLastCommittedURL())) {
    return;
  }

  search_result_extractor_client_.RequestData(
      web_contents(), {continuous_search::mojom::ResultType::kRelatedSearches},
      base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                         OnRelatedSearchesExtracted,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetTimestampFromWebContents(web_contents()),
                     web_contents()->GetLastCommittedURL()));
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsWebContentsObserver."
      "RelatedSearchesExtractRequest",
      true);
}



void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // New navigation. Reset the content visibility score.
  content_visibility_score_ = std::nullopt;
}

void PageContentAnnotationsWebContentsObserver::OnRelatedSearchesExtracted(
    base::Time navigation_timestamp,
    const GURL& navigation_url,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  page_content_annotations_service_->OnRelatedSearchesExtracted(
      navigation_timestamp, navigation_url, status, std::move(results));
}

void PageContentAnnotationsWebContentsObserver::OnPageContentAnnotated(
    const HistoryVisit& annotated_visit,
    const PageContentAnnotationsResult& result) {
  if (GetTimestampFromWebContents(web_contents()) !=
          annotated_visit.nav_entry_timestamp ||
      web_contents()->GetLastCommittedURL() != annotated_visit.url) {
    return;
  }

  content_visibility_score_ = result.GetContentVisibilityScore();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace page_content_annotations
