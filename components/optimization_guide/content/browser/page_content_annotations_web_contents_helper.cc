// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_helper.h"

#include "base/bind.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

PageContentAnnotationsWebContentsHelper::
    PageContentAnnotationsWebContentsHelper(
        content::WebContents* web_contents,
        PageContentAnnotationsService* page_content_annotations_service)
    : web_contents_(web_contents),
      page_content_annotations_service_(page_content_annotations_service),
      max_size_for_text_dump_(features::MaxSizeForPageContentTextDump()) {
  DCHECK(page_content_annotations_service_);

  // Make sure we always attach ourselves to a PageTextObserver.
  PageTextObserver* observer =
      PageTextObserver::GetOrCreateForWebContents(web_contents);
  observer->AddConsumer(this);
}

PageContentAnnotationsWebContentsHelper::
    ~PageContentAnnotationsWebContentsHelper() {
  // Only detach ourselves if PageTextObserver is still alive for
  // |web_contents_|.
  PageTextObserver* observer = PageTextObserver::FromWebContents(web_contents_);
  if (observer)
    observer->RemoveConsumer(this);
}

std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
PageContentAnnotationsWebContentsHelper::MaybeRequestFrameTextDump(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->HasCommitted());

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return nullptr;

  if (!navigation_handle->IsInMainFrame())
    return nullptr;
  // TODO(crbug/1177102): Change above to also annotate content for subframes
  // when API exposes whether it makes up most of the frame.

  // TODO(crbug/1177102): Figure out how to deal with same document navigations.

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
  request->max_size = max_size_for_text_dump_;
  request->events = {mojom::TextDumpEvent::kFirstLayout};
  request->callback = base::BindRepeating(
      &PageContentAnnotationsWebContentsHelper::OnTextDumpReceived,
      weak_ptr_factory_.GetWeakPtr(),
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          navigation_handle->GetWebContents()));
  return request;
}

void PageContentAnnotationsWebContentsHelper::OnTextDumpReceived(
    const HistoryVisit& visit,
    const base::string16& text) {
  page_content_annotations_service_->Annotate(visit, text);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsHelper)

}  // namespace optimization_guide
