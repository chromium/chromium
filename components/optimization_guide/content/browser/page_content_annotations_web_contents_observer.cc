// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

namespace {

// Returns the search query if |url| is a valid Search URL according to
// |template_url_service|.
absl::optional<std::u16string> ExtractSearchTerms(
    const TemplateURLService* template_url_service,
    const GURL& url) {
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  std::u16string search_terms;
  if (default_search_provider &&
      default_search_provider->ExtractSearchTermsFromURL(url, search_terms_data,
                                                         &search_terms) &&
      !search_terms.empty()) {
    return search_terms;
  }
  return absl::nullopt;
}

}  // namespace

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents,
        PageContentAnnotationsService* page_content_annotations_service,
        TemplateURLService* template_url_service)
    : content::WebContentsObserver(web_contents),
      page_content_annotations_service_(page_content_annotations_service),
      template_url_service_(template_url_service),
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
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  optimization_guide::HistoryVisit history_visit = optimization_guide::
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          web_contents(), navigation_handle->GetNavigationId());

  if (google_util::IsGoogleSearchUrl(navigation_handle->GetURL())) {
    // Extract related searches.
    if (optimization_guide::features::ShouldExtractRelatedSearches()) {
      page_content_annotations_service_->ExtractRelatedSearches(history_visit,
                                                                web_contents());
    }

    absl::optional<std::u16string> search_terms =
        ExtractSearchTerms(template_url_service_, navigation_handle->GetURL());
    if (search_terms) {
      const std::u16string& normalized_search_query =
          base::i18n::ToLower(base::CollapseWhitespace(*search_terms, false));
      page_content_annotations_service_->Annotate(
          history_visit, base::UTF16ToUTF8(normalized_search_query));
      return;
    }
  }

  // TODO(crbug/1177102): Remove this title hack once the PageTextObserver works
  // for same-document navigations.
  if (navigation_handle->IsSameDocument()) {
    // Annotate the title instead.
    page_content_annotations_service_->Annotate(
        history_visit, base::UTF16ToUTF8(web_contents()->GetTitle()));
  }
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

  if (navigation_handle->IsSameDocument())
    return nullptr;

  if (google_util::IsGoogleSearchUrl(navigation_handle->GetURL()))
    return nullptr;

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
  request->max_size = max_size_for_text_dump_;
  request->events = {mojom::TextDumpEvent::kFirstLayout};
  request->dump_amp_subframes = true;
  request->callback = base::BindOnce(
      &PageContentAnnotationsWebContentsObserver::OnTextDumpReceived,
      weak_ptr_factory_.GetWeakPtr(),
      PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
          navigation_handle->GetWebContents(),
          navigation_handle->GetNavigationId()));
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace optimization_guide
