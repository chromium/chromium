// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"
#endif

namespace optimization_guide {

namespace {

void LogPageContentAnnotationsStorageStatus(
    PageContentAnnotationsStorageStatus status) {
  DCHECK_NE(status, PageContentAnnotationsStorageStatus::kUnknown);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      status);
}

}  // namespace

PageContentAnnotationsService::PageContentAnnotationsService(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    history::HistoryService* history_service)
    : last_annotated_history_visits_(
          features::MaxContentAnnotationRequestsCached()) {
  DCHECK(optimization_guide_model_provider);
  DCHECK(history_service);
  history_service_ = history_service;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      optimization_guide_model_provider);
#endif
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit,
                                             const std::string& text) {
  if (last_annotated_history_visits_.Peek(visit) !=
      last_annotated_history_visits_.end()) {
    // We have already been requested to annotate this visit, so don't submit
    // for re-annotation.
    return;
  }
  last_annotated_history_visits_.Put(visit, true);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->Annotate(
      text,
      base::BindOnce(&PageContentAnnotationsService::OnPageContentAnnotated,
                     weak_ptr_factory_.GetWeakPtr(), visit));
#endif
}

void PageContentAnnotationsService::ExtractRelatedSearches(
    const HistoryVisit& visit,
    content::WebContents* web_contents) {
  search_result_extractor_client_.RequestData(
      web_contents, {continuous_search::mojom::ResultType::kRelatedSearches},
      base::BindOnce(&PageContentAnnotationsService::OnRelatedSearchesExtracted,
                     weak_ptr_factory_.GetWeakPtr(), visit));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PageContentAnnotationsService::OnPageContentAnnotated(
    const HistoryVisit& visit,
    const absl::optional<history::VisitContentModelAnnotations>&
        content_annotations) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      content_annotations.has_value());
  if (!content_annotations)
    return;

  if (!features::ShouldWriteContentAnnotationsToHistoryService())
    return;

  QueryURL(visit,
           base::BindOnce(
               &history::HistoryService::AddContentModelAnnotationsForVisit,
               history_service_->AsWeakPtr(), *content_annotations));
}
#endif

void PageContentAnnotationsService::OnRelatedSearchesExtracted(
    const HistoryVisit& visit,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  const bool success =
      status == continuous_search::SearchResultExtractorClientStatus::kSuccess;
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotationsService."
      "RelatedSearchesExtracted",
      success);

  if (!success) {
    return;
  }

  std::vector<std::string> related_searches;
  for (const auto& group : results->groups) {
    if (group->type != continuous_search::mojom::ResultType::kRelatedSearches) {
      continue;
    }
    std::transform(std::begin(group->results), std::end(group->results),
                   std::back_inserter(related_searches),
                   [](const continuous_search::mojom::SearchResultPtr& result) {
                     return base::UTF16ToUTF8(
                         base::CollapseWhitespace(result->title, true));
                   });
    break;
  }

  if (related_searches.empty()) {
    return;
  }

  if (!features::ShouldWriteContentAnnotationsToHistoryService()) {
    return;
  }

  QueryURL(visit,
           base::BindOnce(&history::HistoryService::AddRelatedSearchesForVisit,
                          history_service_->AsWeakPtr(), related_searches));
}

void PageContentAnnotationsService::QueryURL(
    const HistoryVisit& visit,
    PersistAnnotationsCallback callback) {
  history_service_->QueryURL(
      visit.url, /*want_visits=*/true,
      base::BindOnce(&PageContentAnnotationsService::OnURLQueried,
                     weak_ptr_factory_.GetWeakPtr(), visit,
                     std::move(callback)),
      &history_service_task_tracker_);
}

void PageContentAnnotationsService::OnURLQueried(
    const HistoryVisit& visit,
    PersistAnnotationsCallback callback,
    history::QueryURLResult url_result) {
  if (!url_result.success) {
    LogPageContentAnnotationsStorageStatus(
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl);
    return;
  }

  bool did_store_content_annotations = false;
  for (const auto& visit_for_url : url_result.visits) {
    if (visit.nav_entry_timestamp != visit_for_url.visit_time)
      continue;

    std::move(callback).Run(visit_for_url.visit_id);

    did_store_content_annotations = true;
    break;
  }
  LogPageContentAnnotationsStorageStatus(
      did_store_content_annotations ? kSuccess : kSpecificVisitForUrlNotFound);
}

absl::optional<int64_t>
PageContentAnnotationsService::GetPageTopicsModelVersion() const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return model_manager_->GetPageTopicsModelVersion();
#else
  return absl::nullopt;
#endif
}

// static
HistoryVisit PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents) {
  HistoryVisit visit = {
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetLastCommittedURL()};
  return visit;
}

}  // namespace optimization_guide
