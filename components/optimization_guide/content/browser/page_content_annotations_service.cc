// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "components/history/core/browser/history_service.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/core/local_page_entities_metadata_provider.h"
#include "components/optimization_guide/core/noisy_metrics_recorder.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
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

const char* kRandomWords[] = {
    "interesting", "chunky",    "maniacal", "tickle",   "lettuce",
    "obsequious",  "stir",      "bless",    "colossal", "squealing",
    "elegant",     "ambitious", "eight",    "frighten", "descriptive",
    "pretty",      "curly",     "regular",  "uneven",   "heap",
};
const size_t kCountRandomWords = 20;

}  // namespace

PageContentAnnotationsService::PageContentAnnotationsService(
    const std::string& application_locale,
    OptimizationGuideModelProvider* optimization_guide_model_provider,
    history::HistoryService* history_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : last_annotated_history_visits_(
          features::MaxContentAnnotationRequestsCached()),
      annotated_text_cache_(features::MaxVisitAnnotationCacheSize()) {
  DCHECK(optimization_guide_model_provider);
  DCHECK(history_service);
  history_service_ = history_service;
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
      application_locale, optimization_guide_model_provider);
  annotator_ = model_manager_.get();
#endif

  if (features::UseLocalPageEntitiesMetadataProvider()) {
    local_page_entities_metadata_provider_ =
        std::make_unique<LocalPageEntitiesMetadataProvider>();
    local_page_entities_metadata_provider_->Initialize(
        database_provider, database_dir, background_task_runner);
  }

  if (features::BatchAnnotationsValidationEnabled()) {
    // Normally the caller would do this, but we are our own caller.
    RequestAndNotifyWhenModelAvailable(
        features::BatchAnnotationsValidationUsePageTopics()
            ? AnnotationType::kPageTopics
            : AnnotationType::kContentVisibility,
        base::DoNothing());

    validation_timer_ = std::make_unique<base::OneShotTimer>(
        base::DefaultTickClock::GetInstance());
    validation_timer_->Start(
        FROM_HERE, features::BatchAnnotationValidationStartupDelay(),
        base::BindRepeating(
            &PageContentAnnotationsService::RunBatchAnnotationValidation,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

PageContentAnnotationsService::~PageContentAnnotationsService() = default;

void PageContentAnnotationsService::Annotate(const HistoryVisit& visit) {
  if (last_annotated_history_visits_.Peek(visit) !=
      last_annotated_history_visits_.end()) {
    // We have already been requested to annotate this visit, so don't submit
    // for re-annotation.
    return;
  }
  last_annotated_history_visits_.Put(visit, true);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!visit.text_to_annotate)
    return;
  // Used for testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      "PageContentAnnotations.AnnotateVisit.AnnotationRequested", true);

  auto it = annotated_text_cache_.Peek(*visit.text_to_annotate);
  if (it != annotated_text_cache_.end()) {
    // We have annotations the text for this visit, so return that immediately
    // rather than re-executing the model.
    //
    // TODO(crbug.com/1291275): If the model was updated, the cached value could
    // be stale so we should invalidate the cache on model updates.
    OnPageContentAnnotated(visit, it->second);
    base::UmaHistogramBoolean(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        true);
    return;
  }
  if (switches::ShouldLogPageContentAnnotationsInput()) {
    LOG(ERROR) << "Adding annotation job: \n"
               << "URL: " << visit.url << "\n"
               << "Text: " << visit.text_to_annotate.value_or(std::string());
  }
  visits_to_annotate_.emplace_back(visit);
  base::UmaHistogramBoolean(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false);
  if (visits_to_annotate_.size() >= features::AnnotateVisitBatchSize()) {
    if (current_visit_annotation_batch_.empty()) {
      // Used for testing.
      LOCAL_HISTOGRAM_BOOLEAN(
          "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true);
      current_visit_annotation_batch_ = std::move(visits_to_annotate_);
      AnnotateVisitBatch();
      return;
    }
    // The queue is full and an batch annotation is actively being done so
    // we will remove the "oldest" visit.
    visits_to_annotate_.erase(visits_to_annotate_.begin());
    // Used for testing.
    LOCAL_HISTOGRAM_BOOLEAN(
        "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", true);
  }
  // Used for testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      "PageContentAnnotations.AnnotateVisit.AnnotationRequestQueued", true);
#endif
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void PageContentAnnotationsService::AnnotateVisitBatch() {
  DCHECK(!current_visit_annotation_batch_.empty());

  if (switches::StopHistoryVisitBatchAnnotateForTesting()) {
    // Code beyond this is tested in multiple places. This just ensures the
    // calls up to this point can be more easily configured.
    return;
  }

  if (current_visit_annotation_batch_.empty()) {
    return;
  }
  auto visit = current_visit_annotation_batch_.back();
  DCHECK(visit.text_to_annotate);
  if (visit.text_to_annotate) {
    model_manager_->Annotate(
        *(visit.text_to_annotate),
        base::BindOnce(&PageContentAnnotationsService::OnBatchVisitAnnotated,
                       weak_ptr_factory_.GetWeakPtr(), visit));
  }
}

void PageContentAnnotationsService::OnBatchVisitAnnotated(
    const HistoryVisit& visit,
    const absl::optional<history::VisitContentModelAnnotations>&
        content_annotations) {
  OnPageContentAnnotated(visit, content_annotations);
  DCHECK_EQ(visit.navigation_id,
            current_visit_annotation_batch_.back().navigation_id);
  current_visit_annotation_batch_.pop_back();
  if (!current_visit_annotation_batch_.empty()) {
    AnnotateVisitBatch();
  }
}
#endif

void PageContentAnnotationsService::OverridePageContentAnnotatorForTesting(
    PageContentAnnotator* annotator) {
  annotator_ = annotator;
}

// static
std::string PageContentAnnotationsService::StringInputForPageTopicsDomain(
    const GURL& url) {
  std::string domain = base::ToLowerASCII(url.host());

  // Strip the 'www.' if it exists.
  if (base::StartsWith(domain, "www.")) {
    domain = domain.substr(4);
  }

  for (char c : std::vector<char>{'-', '_', '.', '+'}) {
    std::replace(domain.begin(), domain.end(), c, ' ');
  }

  return domain;
}

void PageContentAnnotationsService::BatchAnnotatePageTopics(
    BatchAnnotationCallback callback,
    const std::vector<GURL>& inputs) {
  std::vector<std::string> domains;
  for (const GURL& url : inputs) {
    domains.emplace_back(StringInputForPageTopicsDomain(url));
  }

  if (!annotator_) {
    std::move(callback).Run(CreateEmptyBatchAnnotationResults(domains));
    return;
  }

  annotator_->Annotate(std::move(callback), domains,
                       AnnotationType::kPageTopics);
}

void PageContentAnnotationsService::BatchAnnotate(
    BatchAnnotationCallback callback,
    const std::vector<std::string>& inputs,
    AnnotationType annotation_type) {
  DCHECK_NE(annotation_type, AnnotationType::kPageTopics)
      << "Please use |BatchAnnotatePageTopics| instead";

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

void PageContentAnnotationsService::RequestAndNotifyWhenModelAvailable(
    AnnotationType type,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  DCHECK(model_manager_);
  model_manager_->RequestAndNotifyWhenModelAvailable(type, std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

void PageContentAnnotationsService::PersistSearchMetadata(
    const HistoryVisit& visit,
    const SearchMetadata& search_metadata) {
  QueryURL(visit,
           base::BindOnce(&history::HistoryService::AddSearchMetadataForVisit,
                          history_service_->AsWeakPtr(),
                          search_metadata.normalized_url,
                          search_metadata.search_terms));
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

  if (annotated_text_cache_.Peek(*visit.text_to_annotate) ==
      annotated_text_cache_.end()) {
    annotated_text_cache_.Put(*visit.text_to_annotate, *content_annotations);
  }
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
  if (features::UseLocalPageEntitiesMetadataProvider()) {
    DCHECK(local_page_entities_metadata_provider_);
    local_page_entities_metadata_provider_->GetMetadataForEntityId(
        entity_id, std::move(callback));
    return;
  }

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

void PageContentAnnotationsService::RunBatchAnnotationValidation() {
  DCHECK(features::BatchAnnotationsValidationEnabled());
  DCHECK(validation_timer_);
  validation_timer_.reset();

  std::vector<std::string> dummy_inputs;
  dummy_inputs.reserve(features::BatchAnnotationsValidationBatchSize());
  for (size_t i = 0; i < features::BatchAnnotationsValidationBatchSize(); i++) {
    const char* word1 = kRandomWords[base::RandGenerator(kCountRandomWords)];
    const char* word2 = kRandomWords[base::RandGenerator(kCountRandomWords)];
    dummy_inputs.emplace_back(base::StringPrintf("%s-%s.com", word1, word2));
  }

  LOCAL_HISTOGRAM_COUNTS_100(
      "OptimizationGuide.PageContentAnnotationsService.ValidationRun",
      dummy_inputs.size());

  if (!features::BatchAnnotationsValidationUsePageTopics()) {
    BatchAnnotate(base::DoNothing(), dummy_inputs,
                  AnnotationType::kContentVisibility);
    return;
  }

  std::vector<GURL> urls;
  for (const std::string& domain : dummy_inputs) {
    urls.emplace_back(GURL("https://" + domain));
  }
  BatchAnnotatePageTopics(base::DoNothing(), urls);
}

// static
HistoryVisit PageContentAnnotationsService::CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents,
    int64_t navigation_id) {
  HistoryVisit visit(
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetLastCommittedURL(), navigation_id);
  return visit;
}

HistoryVisit::HistoryVisit() = default;

HistoryVisit::HistoryVisit(base::Time nav_entry_timestamp,
                           GURL url,
                           int64_t navigation_id) {
  this->nav_entry_timestamp = nav_entry_timestamp;
  this->url = url;
  this->navigation_id = navigation_id;
}

HistoryVisit::~HistoryVisit() = default;
HistoryVisit::HistoryVisit(const HistoryVisit&) = default;

}  // namespace optimization_guide
