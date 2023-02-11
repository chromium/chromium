// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"

namespace optimization_guide {

namespace {

// Creates a HistoryVisit based on the current state of |web_contents|.
HistoryVisit CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents,
    int64_t navigation_id) {
  HistoryVisit visit(
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetLastCommittedURL(), navigation_id);
  return visit;
}

// Data scoped to a single page. PageData has the same lifetime as the page's
// main document. Contains information for whether we annotated the title for
// the page yet.
class PageData : public content::PageUserData<PageData> {
 public:
  explicit PageData(content::Page& page) : PageUserData(page) {}
  PageData(const PageData&) = delete;
  PageData& operator=(const PageData&) = delete;
  ~PageData() override = default;

  int64_t navigation_id() const { return navigation_id_; }
  void set_navigation_id(int64_t navigation_id) {
    navigation_id_ = navigation_id;
  }

  bool annotation_was_requested() const { return annotation_was_requested_; }
  void set_annotation_was_requested() { annotation_was_requested_ = true; }

  PAGE_USER_DATA_KEY_DECL();

 private:
  int64_t navigation_id_ = 0;
  bool annotation_was_requested_ = false;
};

PAGE_USER_DATA_KEY_IMPL(PageData);

}  // namespace

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents,
        PageContentAnnotationsService* page_content_annotations_service,
        TemplateURLService* template_url_service,
        OptimizationGuideDecider* optimization_guide_decider,
        prerender::NoStatePrefetchManager* no_state_prefetch_manager)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageContentAnnotationsWebContentsObserver>(
          *web_contents),
      page_content_annotations_service_(page_content_annotations_service),
      salient_image_retriever_(
          page_content_annotations_service_->optimization_guide_logger()),
      template_url_service_(template_url_service),
      optimization_guide_decider_(optimization_guide_decider),
      no_state_prefetch_manager_(no_state_prefetch_manager) {
  DCHECK(page_content_annotations_service_);

  std::vector<proto::OptimizationType> optimization_types;
  if (features::RemotePageMetadataEnabled()) {
    optimization_types.emplace_back(proto::PAGE_ENTITIES);
  }
  if (features::ShouldPersistSalientImageMetadata()) {
    optimization_types.emplace_back(proto::SALIENT_IMAGE);
  }
  if (optimization_guide_decider_ && !optimization_types.empty()) {
    optimization_guide_decider_->RegisterOptimizationTypes(optimization_types);
  }
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() = default;

void PageContentAnnotationsWebContentsObserver::DidStopLoading() {
  salient_image_retriever_.GetOgImage(web_contents());
}

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  // No-state prefetch does not update history, so don't execute any models for
  // it.
  if (no_state_prefetch_manager_ &&
      no_state_prefetch_manager_->IsWebContentsPrefetching(web_contents())) {
    return;
  }

  // Conditions above here should match what is in HistoryTabHelper.

  PageData* page_data =
      PageData::GetOrCreateForPage(web_contents()->GetPrimaryPage());
  page_data->set_navigation_id(navigation_handle->GetNavigationId());

  optimization_guide::HistoryVisit history_visit =
      CreateHistoryVisitFromWebContents(web_contents(),
                                        navigation_handle->GetNavigationId());
  if (features::RemotePageMetadataEnabled() && optimization_guide_decider_) {
    optimization_guide_decider_->CanApplyOptimizationAsync(
        navigation_handle, proto::PAGE_ENTITIES,
        base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                           OnOptimizationGuideResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), history_visit,
                       proto::PAGE_ENTITIES));
  }
  if (features::ShouldPersistSalientImageMetadata() &&
      optimization_guide_decider_) {
    optimization_guide_decider_->CanApplyOptimizationAsync(
        navigation_handle, proto::SALIENT_IMAGE,
        base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                           OnOptimizationGuideResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), history_visit,
                       proto::SALIENT_IMAGE));
  }

  bool is_google_search_url =
      google_util::IsGoogleSearchUrl(navigation_handle->GetURL());

  // Persist search metadata, if applicable if it's a Google search URL or if
  // it's a search-y URL as determined by the TemplateURLService if the flag is
  // enabled.
  if (template_url_service_ &&
      (is_google_search_url ||
       optimization_guide::features::
           ShouldPersistSearchMetadataForNonGoogleSearches())) {
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentAnnotations."
        "TemplateURLServiceLoadedAtNavigationFinish",
        template_url_service_->loaded());

    auto search_metadata = template_url_service_->ExtractSearchMetadata(
        navigation_handle->GetURL());
    if (search_metadata) {
      if (page_data) {
        page_data->set_annotation_was_requested();
      }
      history_visit.text_to_annotate =
          base::UTF16ToUTF8(search_metadata->search_terms);
      page_content_annotations_service_->Annotate(history_visit);

      if (switches::ShouldLogPageContentAnnotationsInput()) {
        LOG(ERROR) << "Annotating search terms: \n"
                   << "URL: " << navigation_handle->GetURL() << "\n"
                   << "Text: " << *(history_visit.text_to_annotate);
      }

      return;
    }
  }

  // Same-document navigations and reloads do not trigger |TitleWasSet|, so we
  // need to capture the title text here for these cases.
  if (navigation_handle->IsSameDocument() ||
      navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    if (page_data) {
      page_data->set_annotation_was_requested();
    }
    // Annotate the title instead.
    history_visit.text_to_annotate =
        base::UTF16ToUTF8(web_contents()->GetTitle());
    page_content_annotations_service_->Annotate(history_visit);

    if (switches::ShouldLogPageContentAnnotationsInput()) {
      LOG(ERROR) << "Annotating same document navigation: \n"
                 << "URL: " << navigation_handle->GetURL() << "\n"
                 << "Text: " << *(history_visit.text_to_annotate);
    }
  }
}

// This triggers the annotation of titles for pages that are not SRP or
// same document and will only request annotation if the flag to annotate titles
// instead of content is enabled.
void PageContentAnnotationsWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  if (!entry)
    return;

  PageData* page_data = PageData::GetForPage(web_contents()->GetPrimaryPage());
  if (!page_data)
    return;
  if (page_data->annotation_was_requested())
    return;

  page_data->set_annotation_was_requested();
  optimization_guide::HistoryVisit history_visit =
      CreateHistoryVisitFromWebContents(web_contents(),
                                        page_data->navigation_id());
  history_visit.text_to_annotate =
      base::UTF16ToUTF8(entry->GetTitleForDisplay());
  page_content_annotations_service_->Annotate(history_visit);

  if (switches::ShouldLogPageContentAnnotationsInput()) {
    LOG(ERROR) << "Annotating main frame navigation: \n"
               << "URL: " << entry->GetURL() << "\n"
               << "Text: " << *(history_visit.text_to_annotate);
  }
}

void PageContentAnnotationsWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  PageData* page_data = PageData::GetForPage(web_contents()->GetPrimaryPage());
  if (!page_data)
    return;

  optimization_guide::HistoryVisit history_visit =
      CreateHistoryVisitFromWebContents(web_contents(),
                                        page_data->navigation_id());
  bool is_google_search_url =
      google_util::IsGoogleSearchUrl(web_contents()->GetLastCommittedURL());
  if (is_google_search_url &&
      optimization_guide::features::ShouldExtractRelatedSearches()) {
    page_content_annotations_service_->ExtractRelatedSearches(history_visit,
                                                              web_contents());
  }
}

void PageContentAnnotationsWebContentsObserver::
    OnOptimizationGuideResponseReceived(
        const HistoryVisit& history_visit,
        proto::OptimizationType optimization_type,
        OptimizationGuideDecision decision,
        const OptimizationMetadata& metadata) {
  if (decision != OptimizationGuideDecision::kTrue) {
    return;
  }

  switch (optimization_type) {
    case proto::OptimizationType::PAGE_ENTITIES: {
      absl::optional<proto::PageEntitiesMetadata> page_entities_metadata =
          metadata.ParsedMetadata<proto::PageEntitiesMetadata>();
      if (page_entities_metadata) {
        page_content_annotations_service_->PersistRemotePageMetadata(
            history_visit, *page_entities_metadata);
      }
      break;
    }
    case proto::OptimizationType::SALIENT_IMAGE: {
      absl::optional<proto::SalientImageMetadata> salient_image_metadata =
          metadata.ParsedMetadata<proto::SalientImageMetadata>();
      if (salient_image_metadata) {
        page_content_annotations_service_->PersistSalientImageMetadata(
            history_visit, *salient_image_metadata);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace optimization_guide
