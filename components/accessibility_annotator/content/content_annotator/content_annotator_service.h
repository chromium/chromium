// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/accessibility_annotator/content/content_annotator/content_classifier_types.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "url/gurl.h"

namespace optimization_guide {
class ModelQualityLogEntry;
class RemoteModelExecutor;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace translate {
struct LanguageDetectionDetails;
}  // namespace translate

namespace accessibility_annotator {

class ContentClassifier;

class ContentAnnotatorService
    : public KeyedService,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver,
      public page_content_annotations::PageContentExtractionService::Observer,
      public page_content_annotations::PageEmbeddingsService::Observer,
      public passage_embeddings::EmbedderMetadataObserver {
 public:
  static std::unique_ptr<ContentAnnotatorService> Create(
      page_content_annotations::PageContentAnnotationsService&
          page_content_annotations_service,
      page_content_annotations::PageContentExtractionService&
          page_content_extraction_service,
      optimization_guide::RemoteModelExecutor&
          optimization_guide_remote_model_executor,
      page_content_annotations::PageEmbeddingsService& page_embeddings_service,
      AccessibilityAnnotatorBackend& accessibility_annotator_backend,
      passage_embeddings::Embedder* embedder,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider);

  ~ContentAnnotatorService() override;

  ContentAnnotatorService(const ContentAnnotatorService&) = delete;
  ContentAnnotatorService& operator=(const ContentAnnotatorService&) = delete;

  // page_content_annotations::PageContentAnnotationsService::
  //     PageContentAnnotationsObserver:
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

  // Called when the language of the contents of the current page has been
  // determined. This will be called by the ContentAnnotatorTabHelper, which
  // observes the translate::TranslateDriver.
  // Virtual for testing.
  virtual void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details);

  // page_content_annotations::PageContentExtractionService::
  //     Observer:
  void OnPageContentExtracted(
      content::Page& page,
      scoped_refptr<
          const page_content_annotations::RefCountedAnnotatedPageContent>
          page_content) override;

  // page_content_annotations::PageEmbeddingsService::Observer:
  page_content_annotations::PageEmbeddingsService::UsageMode GetUsageMode()
      const override;
  void OnPageEmbeddingsAvailable(content::Page& page) override;

  // passage_embeddings::EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

 protected:
  ContentAnnotatorService(
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
      std::unique_ptr<ContentClassifier> content_classifier);

 private:
  using CacheIterator =
      base::LRUCache<GURL, ContentClassificationInput>::iterator;

  // Returns the iterator to the entry for the given URL, creating a new entry
  // if it does not exist.
  CacheIterator GetOrCreateJoinEntry(const GURL& url);

  // If the data is complete, runs content classification and determines
  // annotation eligibility.
  void MaybeAnnotate(CacheIterator it);

  // Generates annotations for the given visit_id based on the provided
  // `page_context`.
  void GenerateAnnotations(
      optimization_guide::proto::PageContext page_context,
      history::VisitID visit_id,
      AccessibilityAnnotatorBackend::ContentAnnotationsData data);

  // Handles the result of the model execution from `GenerateAnnotations`.
  void HandleModelExecutionResult(
      history::VisitID visit_id,
      AccessibilityAnnotatorBackend::ContentAnnotationsData data,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // `ContentAnnotatorServiceFactory` uses a `DependsOn()` to guarantee that
  // the `raw_ref`s below outlive `ContentAnnotatorService`.
  const raw_ref<page_content_annotations::PageContentAnnotationsService>
      page_content_annotations_service_;

  const raw_ref<optimization_guide::RemoteModelExecutor>
      optimization_guide_remote_model_executor_;

  const raw_ref<page_content_annotations::PageEmbeddingsService>
      page_embeddings_service_;

  const raw_ref<AccessibilityAnnotatorBackend> accessibility_annotator_backend_;

  raw_ptr<passage_embeddings::Embedder> embedder_ = nullptr;

  base::ScopedObservation<
      page_content_annotations::PageContentExtractionService,
      page_content_annotations::PageContentExtractionService::Observer>
      page_content_extraction_service_observation_{this};

  base::ScopedObservation<
      page_content_annotations::PageEmbeddingsService,
      page_content_annotations::PageEmbeddingsService::Observer>
      page_embeddings_service_observation_{this};

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      embedder_metadata_observation_{this};

  // Stores and joins data for URLs that are pending annotation. The cache size
  // is `kContentAnnotatorMaxPendingUrls`. When the cache is full, the last
  // modified entry is evicted. The cache should only go over capacity in the
  // case expected data from an observation does not arrive or the user
  // navigates to several URLs faster than data can be collected for the URL and
  // annotations processed.
  base::LRUCache<GURL, ContentClassificationInput> join_entries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The metadata for the passage embedder.
  passage_embeddings::EmbedderMetadata embedder_metadata_{0, 0};

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<ContentClassifier> content_classifier_;

  base::WeakPtrFactory<ContentAnnotatorService> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_SERVICE_H_
