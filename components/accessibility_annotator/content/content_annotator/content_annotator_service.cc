// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier.h"
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

ContentAnnotatorService::ContentAnnotatorService(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service,
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    optimization_guide::RemoteModelExecutor&
        optimization_guide_remote_model_executor)
    : page_content_annotations_service_(page_content_annotations_service),
      page_content_extraction_service_(page_content_extraction_service),
      optimization_guide_remote_model_executor_(
          optimization_guide_remote_model_executor),
      join_entries_(kContentAnnotatorMaxPendingUrls.Get()) {
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
  MaybeAnnotate(it);
}

ContentAnnotatorService::CacheIterator
ContentAnnotatorService::GetOrCreateJoinEntry(const GURL& url) {
  CacheIterator it = join_entries_.Get(url);
  if (it != join_entries_.end()) {
    return it;
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
  RunContentClassification(std::move(complete_data));
  // TODO(crbug.com/476434957): Process classification result.
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
