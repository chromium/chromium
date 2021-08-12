// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/bind.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents,
        PageContentAnnotationsService* page_content_annotations_service)
    : content::WebContentsObserver(web_contents),
      page_content_annotations_service_(page_content_annotations_service),
      max_size_for_text_dump_(features::MaxSizeForPageContentTextDump()) {
  DCHECK(page_content_annotations_service_);

  // Make sure we always attach ourselves to a PageTextObserver.
  PageTextObserver* observer =
      PageTextObserver::GetOrCreateForWebContents(web_contents);
  observer->AddConsumer(this);
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() {
  // Only detach ourselves if |web_contents()| as well as PageTextObserver for
  // |web_contents()| are still alive.
  if (!web_contents())
    return;

  PageTextObserver* observer =
      PageTextObserver::FromWebContents(web_contents());
  if (observer)
    observer->RemoveConsumer(this);
}

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!optimization_guide::features::ShouldExtractRelatedSearches()) {
    return;
  }

  if (!google_util::IsGoogleSearchUrl(navigation_handle->GetURL())) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  page_content_annotations_service_->ExtractRelatedSearches(
      optimization_guide::PageContentAnnotationsService::
          CreateHistoryVisitFromWebContents(web_contents()),
      web_contents());
}

std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
PageContentAnnotationsWebContentsObserver::MaybeRequestFrameTextDump(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->HasCommitted());
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  DCHECK(navigation_handle->IsInPrimaryMainFrame());

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return nullptr;

  // TODO(crbug/1177102): Figure out how to deal with same document navigations.

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
  request->max_size = max_size_for_text_dump_;
  request->events = {mojom::TextDumpEvent::kFirstLayout};
  request->dump_amp_subframes = true;
  request->callback = base::BindOnce(
      &PageContentAnnotationsWebContentsObserver::OnTextDumpReceived,
      weak_ptr_factory_.GetWeakPtr(),
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          navigation_handle->GetWebContents()));
  return request;
}

void PageContentAnnotationsWebContentsObserver::OnTextDumpReceived(
    const HistoryVisit& visit,
    const PageTextDumpResult& result) {
  if (result.empty()) {
    return;
  }

  // If the page had AMP frames, then only use that content. Otherwise, use the
  // mainframe.
  if (result.GetAMPTextContent()) {
    page_content_annotations_service_->Annotate(visit,
                                                *result.GetAMPTextContent());
    return;
  }
  page_content_annotations_service_->Annotate(
      visit, *result.GetMainFrameTextContent());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver)

}  // namespace optimization_guide
