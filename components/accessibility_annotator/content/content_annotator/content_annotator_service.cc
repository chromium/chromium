// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/page.h"

namespace accessibility_annotator {

namespace {

bool HasClassifierCategory(
    const std::optional<ContentClassificationResult::Result>& result) {
  return result.has_value() && result->category.has_value();
}

bool PassesSafetyChecks(const ContentClassificationResult& result) {
  return !result.is_sensitive.value_or(true) &&
         result.is_in_target_language.value_or(false);
}

}  // namespace

// static
std::unique_ptr<ContentAnnotatorService> ContentAnnotatorService::Create(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service,
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    optimization_guide::RemoteModelExecutor&
        optimization_guide_remote_model_executor) {
  std::unique_ptr<ContentClassifier> content_classifier =
      ContentClassifier::Create();
  if (!content_classifier) {
    return nullptr;
  }
  return base::WrapUnique(new ContentAnnotatorService(
      page_content_annotations_service, page_content_extraction_service,
      optimization_guide_remote_model_executor, std::move(content_classifier)));
}

ContentAnnotatorService::ContentAnnotatorService(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service,
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    optimization_guide::RemoteModelExecutor&
        optimization_guide_remote_model_executor,
    std::unique_ptr<ContentClassifier> content_classifier)
    : page_content_annotations_service_(page_content_annotations_service),
      page_content_extraction_service_(page_content_extraction_service),
      optimization_guide_remote_model_executor_(
          optimization_guide_remote_model_executor),
      join_entries_(kContentAnnotatorMaxPendingUrls.Get()),
      content_classifier_(std::move(content_classifier)) {
  CHECK(content_classifier_);
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
  page_content_extraction_service_observation_.Observe(
      &page_content_extraction_service_.get());
}

ContentAnnotatorService::~ContentAnnotatorService() {
  page_content_annotations_service_->RemoveObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

void ContentAnnotatorService::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(result.GetType() ==
        page_content_annotations::AnnotationType::kContentVisibility);
  CacheIterator it = GetOrCreateJoinEntry(visit.url);
  // Invert the visibility score to get a sensitivity score.
  it->second.sensitivity_score = 1.0 - result.GetContentVisibilityScore();
  it->second.navigation_timestamp = visit.nav_entry_timestamp;
  MaybeAnnotate(it);
}

void ContentAnnotatorService::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CacheIterator it = GetOrCreateJoinEntry(details.url);
  it->second.adopted_language = details.adopted_language;
  MaybeAnnotate(it);
}

void ContentAnnotatorService::OnPageContentExtracted(
    content::Page& page,
    scoped_refptr<
        const page_content_annotations::RefCountedAnnotatedPageContent>
        page_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(page_content);

  CacheIterator it =
      GetOrCreateJoinEntry(page.GetMainDocument().GetLastCommittedURL());
  if (page_content->data.has_main_frame_data()) {
    it->second.page_title = page_content->data.main_frame_data().title();
  }

  it->second.annotated_page_content = std::move(page_content);
  it->second.ukm_source_id = page.GetMainDocument().GetPageUkmSourceId();
  MaybeAnnotate(it);
}

ContentAnnotatorService::CacheIterator
ContentAnnotatorService::GetOrCreateJoinEntry(const GURL& url) {
  CacheIterator it = join_entries_.Get(url);
  if (it != join_entries_.end()) {
    return it;
  }

  // Check if the cache is full.
  // The least recently used entry will be evicted when adding a new entry.
  if (join_entries_.size() >= join_entries_.max_size()) {
    join_entries_.rbegin()->second.LogMissingFields();
  }

  return join_entries_.Put(url, ContentClassificationInput(url));
}

void ContentAnnotatorService::MaybeAnnotate(CacheIterator it) {
  if (!it->second.IsComplete()) {
    return;
  }
  ContentClassificationInput complete_data = std::move(it->second);
  join_entries_.Erase(it);
  // TODO(crbug.com/475859254): Move this call to a separate task/sequence as
  // needed.
  ContentClassificationResult result =
      content_classifier_->Classify(complete_data);

  // Annotation proceeds if safety checks pass and at least one value classifier
  // finds relevant content.
  bool reached_annotation =
      (HasClassifierCategory(result.title_keyword_result) ||
       HasClassifierCategory(result.url_match_result)) &&
      PassesSafetyChecks(result);
  base::UmaHistogramBoolean("AccessibilityAnnotator.FullAnnotationReached",
                            reached_annotation);
  // TODO(crbug.com/485675335): Process classification result with gateway flag
  // to full annotation.
}

// TODO(crbug.com/482383206): Update to handle APC ingestion.
void ContentAnnotatorService::GenerateAnnotations(std::string prompt) {
  optimization_guide::proto::StringValue request;
  request.set_value(std::move(prompt));

  // TODO(crbug.com/482383206): Use prod feature key once available.
  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTest, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&ContentAnnotatorService::HandleModelExecutionResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContentAnnotatorService::HandleModelExecutionResult(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::StringValue> string_value =
        optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::StringValue>(result.response.value());
    if (string_value) {
      // TODO(crbug.com/482383206): Handle model execution response.
      DVLOG(1) << "Model execution result: " << string_value->value();
    }
  }
}

}  // namespace accessibility_annotator
