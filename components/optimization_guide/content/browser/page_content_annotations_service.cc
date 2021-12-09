// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/noisy_metrics_recorder.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Record the visibility score of the provided visit as a RAPPOR-style record to
// UKM.
void MaybeRecordVisibilityUKM(
    const HistoryVisit& visit,
    const absl::optional<history::VisitContentModelAnnotations>&
        content_annotations) {
  if (!content_annotations)
    return;

  if (content_annotations->visibility_score < 0)
    return;

  int64_t score =
      static_cast<int64_t>(100 * content_annotations->visibility_score);
  // We want 2^|num_bits| buckets, linearly spaced.
  uint32_t num_buckets =
      std::pow(2, optimization_guide::features::NumBitsForRAPPORMetrics());
  DCHECK_GT(num_buckets, 0u);
  float bucket_size = 100.0 / num_buckets;
  uint32_t bucketed_score = static_cast<uint32_t>(floor(score / bucket_size));
  DCHECK_LE(bucketed_score, num_buckets);
  uint32_t noisy_score = NoisyMetricsRecorder().GetNoisyMetric(
      optimization_guide::features::NoiseProbabilityForRAPPORMetrics(),
      bucketed_score, optimization_guide::features::NumBitsForRAPPORMetrics());
  ukm::SourceId ukm_source_id = ukm::ConvertToSourceId(
      visit.navigation_id, ukm::SourceIdType::NAVIGATION_ID);

  ukm::builders::PageContentAnnotations(ukm_source_id)
      .SetVisibilityScore(static_cast<int64_t>(noisy_score))
      .Record(ukm::UkmRecorder::Get());
}
#endif /* BUILDFLAG(BUILD_WITH_TFLITE_LIB) */

}  // namespace

PageContentAnnotationsService::PageContentAnnotationsService(
    const std::string& application_locale,
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    history::HistoryService* history_service)
    : last_annotated_history_visits_(
          features::MaxContentAnnotationRequestsCached()) {
  DCHECK(optimization_guide_model_provider);
  DCHECK(history_service);
  history_service_ = history_service;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      application_locale, optimization_guide_model_provider);
  annotator_ = model_manager_.get();
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

void PageContentAnnotationsService::OverridePageContentAnnotatorForTesting(
    PageContentAnnotator* annotator) {
  annotator_ = annotator;
}

void PageContentAnnotationsService::BatchAnnotate(
    BatchAnnotationCallback callback,
    const std::vector<std::string>& inputs,
    AnnotationType annotation_type) {
  if (!annotator_) {
    std::move(callback).Run(CreateEmptyBatchAnnotationResults(inputs));
    return;
  }
  annotator_->Annotate(std::move(callback), inputs, annotation_type);
}

absl::optional<ModelInfo> PageContentAnnotationsService::GetModelInfoForType(
    AnnotationType type) const {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  DCHECK(model_manager_);
  return model_manager_->GetModelInfoForType(type);
#else
  return absl::nullopt;
#endif
}

void PageContentAnnotationsService::NotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  DCHECK(model_manager_);
  model_manager_->NotifyWhenModelAvailable(type, std::move(callback));
#else
  std::move(callback).Run(false);
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

  MaybeRecordVisibilityUKM(visit, content_annotations);

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

void PageContentAnnotationsService::GetMetadataForEntityId(
    const std::string& entity_id,
    EntityMetadataRetrievedCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_->GetMetadataForEntityId(entity_id, std::move(callback));
#else
  std::move(callback).Run(absl::nullopt);
#endif
}

void PageContentAnnotationsService::PersistRemotePageEntities(
    const HistoryVisit& history_visit,
    const std::vector<history::VisitContentModelAnnotations::Category>&
        entities) {
  history::VisitContentModelAnnotations annotations;
  annotations.entities = entities;
  QueryURL(history_visit,
           base::BindOnce(
               &history::HistoryService::AddContentModelAnnotationsForVisit,
               history_service_->AsWeakPtr(), annotations));
}

// static
HistoryVisit PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents,
    int64_t navigation_id) {
  HistoryVisit visit = {
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetLastCommittedURL(), navigation_id};
  return visit;
}

}  // namespace optimization_guide
