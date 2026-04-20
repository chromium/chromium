// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/content_annotator/content_classifier.h"
#include "components/accessibility_annotator/core/content_annotator/content_classifier_types.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/page.h"

namespace accessibility_annotator {

namespace {

bool HasClassifierCategory(
    const std::optional<ContentClassificationResult::Result>& result) {
  return result.has_value() && result->category.has_value();
}

bool PassesSafetyChecks(const ContentClassificationResult& result) {
  // If language check isn't enabled, default to passing.
  return !result.is_sensitive.value_or(true) &&
         result.is_in_target_language.value_or(true);
}

base::DictValue ContentClassificationResultToDict(
    const ContentClassificationResult& result) {
  base::DictValue dict;
  if (result.title_keyword_result.has_value()) {
    dict.Set("title_keyword_result",
             result.title_keyword_result->category.value_or("none"));
  }
  if (result.url_match_result.has_value()) {
    dict.Set("url_match_result",
             result.url_match_result->category.value_or("none"));
  }
  if (result.semantic_match_result.has_value()) {
    dict.Set("semantic_match_result",
             result.semantic_match_result->category.value_or("none"));
  }
  return dict;
}

}  // namespace

// static
std::unique_ptr<ContentAnnotatorService> ContentAnnotatorService::Create(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service,
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    optimization_guide::RemoteModelExecutor&
        optimization_guide_remote_model_executor,
    page_content_annotations::PageEmbeddingsService& page_embeddings_service,
    AccessibilityAnnotatorBackend& accessibility_annotator_backend,
    passage_embeddings::Embedder* embedder,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider) {
  std::unique_ptr<ContentClassifier> content_classifier =
      ContentClassifier::Create(embedder);
  if (!content_classifier) {
    return nullptr;
  }
  return base::WrapUnique(new ContentAnnotatorService(
      page_content_annotations_service, page_content_extraction_service,
      optimization_guide_remote_model_executor, page_embeddings_service,
      accessibility_annotator_backend, embedder, embedder_metadata_provider,
      std::move(content_classifier)));
}

ContentAnnotatorService::ContentAnnotatorService(
    page_content_annotations::PageContentAnnotationsService&
        page_content_annotations_service,
    page_content_annotations::PageContentExtractionService&
        page_content_extraction_service,
    optimization_guide::RemoteModelExecutor&
        optimization_guide_remote_model_executor,
    page_content_annotations::PageEmbeddingsService& page_embeddings_service,
    AccessibilityAnnotatorBackend& accessibility_annotator_backend,
    passage_embeddings::Embedder* embedder,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    std::unique_ptr<ContentClassifier> content_classifier)
    : page_content_annotations_service_(page_content_annotations_service),
      optimization_guide_remote_model_executor_(
          optimization_guide_remote_model_executor),
      page_embeddings_service_(page_embeddings_service),
      accessibility_annotator_backend_(accessibility_annotator_backend),
      embedder_(embedder),
      join_entries_(features::kContentAnnotatorMaxPendingUrls.Get()),
      content_classifier_(std::move(content_classifier)) {
  CHECK(content_classifier_);
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
  page_content_extraction_service_observation_.Observe(
      &page_content_extraction_service);
  page_embeddings_service_observation_.Observe(&page_embeddings_service_.get());
  if (embedder_metadata_provider) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }
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
  it->second.visit_id = visit.visit_id;
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

  std::optional<int> tab_id;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  if (web_contents) {
    SessionID id = sessions::SessionTabHelper::IdForTab(web_contents);
    if (id.is_valid()) {
      tab_id = id.id();
    }
  }

  CacheIterator it =
      GetOrCreateJoinEntry(page.GetMainDocument().GetLastCommittedURL());
  if (page_content->data.has_main_frame_data()) {
    it->second.page_title = page_content->data.main_frame_data().title();
  }

  it->second.annotated_page_content = std::move(page_content);
  it->second.ukm_source_id = page.GetMainDocument().GetPageUkmSourceId();
  it->second.tab_id = tab_id;
  MaybeAnnotate(it);
}

page_content_annotations::PageEmbeddingsService::UsageMode
ContentAnnotatorService::GetUsageMode() const {
  return page_content_annotations::PageEmbeddingsService::UsageMode::
      kContinuous;
}

void ContentAnnotatorService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (embedder_metadata_.IsValid() || !metadata.IsValid()) {
    // TODO(crbug.com/489566579): Handle runtime model changes.
    return;
  }
  embedder_metadata_ = metadata;
  content_classifier_->OnEmbedderModelChanged();
}

void ContentAnnotatorService::OnPageEmbeddingsAvailable(content::Page& page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<page_content_annotations::PassageEmbedding> embeddings =
      page_embeddings_service_->GetEmbeddings(page);
  if (embeddings.empty()) {
    return;
  }

  const GURL& url = page.GetMainDocument().GetLastCommittedURL();
  CacheIterator it = GetOrCreateJoinEntry(url);

  for (const auto& embedding : embeddings) {
    // TODO(crbug.com/487779615): Add support for body text embeddings.
    if (embedding.passage.second ==
        page_content_annotations::EmbeddingPassageType::kTitle) {
      it->second.page_title_embedding = embedding.embedding;
      break;
    }
  }

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
       HasClassifierCategory(result.url_match_result) ||
       HasClassifierCategory(result.semantic_match_result)) &&
      PassesSafetyChecks(result);
  base::UmaHistogramBoolean("AccessibilityAnnotator.FullAnnotationReached",
                            reached_annotation);
  if (reached_annotation &&
      features::kContentAnnotatorEnableFullAnnotation.Get()) {
    base::DictValue classifier_values =
        ContentClassificationResultToDict(result);

    optimization_guide::proto::PageContext page_context;
    page_context.set_url(complete_data.url.spec());
    page_context.set_title(complete_data.page_title.value());
    *page_context.mutable_annotated_page_content() =
        complete_data.annotated_page_content->data;

    AccessibilityAnnotatorBackend::ContentAnnotationsData data;
    data.page_title = complete_data.page_title.value();
    data.url = complete_data.url;
    data.tab_id = complete_data.tab_id;
    data.navigation_timestamp = complete_data.navigation_timestamp.value();
    data.classifier_results = std::move(classifier_values);

    GenerateAnnotations(std::move(page_context), complete_data.visit_id.value(),
                        std::move(data));
  }
}

void ContentAnnotatorService::GenerateAnnotations(
    optimization_guide::proto::PageContext page_context,
    history::VisitID visit_id,
    AccessibilityAnnotatorBackend::ContentAnnotationsData data) {
  optimization_guide::proto::ContentAnnotationRequest request;
  *request.mutable_page_context() = std::move(page_context);

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContentAnnotation,
      std::move(request),
      {.execution_timeout = features::kContentAnnotatorAnnotationTimeout.Get()},
      base::BindOnce(&ContentAnnotatorService::HandleModelExecutionResult,
                     weak_ptr_factory_.GetWeakPtr(), visit_id,
                     std::move(data)));
}

void ContentAnnotatorService::HandleModelExecutionResult(
    history::VisitID visit_id,
    AccessibilityAnnotatorBackend::ContentAnnotationsData data,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (visit_id == history::kInvalidVisitID) {
    return;
  }

  std::optional<optimization_guide::proto::ContentAnnotationResponse> response =
      base::OptionalFromExpected(std::move(result.response))
          .and_then([](const optimization_guide::proto::Any& any) {
            return optimization_guide::ParsedAnyMetadata<
                optimization_guide::proto::ContentAnnotationResponse>(any);
          });

  if (!response) {
    return;
  }

  if (response->has_content_annotation()) {
    // Store ContentAnnotation if the response has one.
    std::optional<optimization_guide::proto::ContentAnnotation>
        content_annotation = response->content_annotation();
    if (content_annotation.has_value()) {
      data.content_annotation = std::move(*content_annotation);
      accessibility_annotator_backend_->SetContentAnnotationsCacheData(
          visit_id, std::move(data));
    }
  }
}

}  // namespace accessibility_annotator
