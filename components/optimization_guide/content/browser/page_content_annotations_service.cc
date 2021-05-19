// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/metrics/histogram_functions.h"
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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
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
#endif

PageContentAnnotationsService::PageContentAnnotationsService(
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    history::HistoryService* history_service) {
  DCHECK(optimization_guide_model_provider);
  DCHECK(history_service);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  history_service_ = history_service;
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      optimization_guide_model_provider);
#endif
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit,
                                             const std::string& text) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->Annotate(
      text,
      base::BindOnce(&PageContentAnnotationsService::OnPageContentAnnotated,
                     weak_ptr_factory_.GetWeakPtr(), visit));
#endif
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

  history_service_->QueryURL(
      visit.url, /*want_visits=*/true,
      base::BindOnce(&PageContentAnnotationsService::OnURLQueried,
                     weak_ptr_factory_.GetWeakPtr(), visit,
                     *content_annotations),
      &history_service_task_tracker_);
}
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PageContentAnnotationsService::OnURLQueried(
    const HistoryVisit& visit,
    const history::VisitContentModelAnnotations& content_annotations,
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

    history_service_->AddContentModelAnnotationsForVisit(visit_for_url.visit_id,
                                                         content_annotations);

    did_store_content_annotations = true;
    break;
  }
  LogPageContentAnnotationsStorageStatus(
      did_store_content_annotations ? kSuccess : kSpecificVisitForUrlNotFound);
}
#endif

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
