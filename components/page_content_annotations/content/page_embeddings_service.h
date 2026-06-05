// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_EMBEDDINGS_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace page_content_annotations {

class PageContentExtractionService;

class PageEmbeddingsService : public KeyedService,
                              public PageContentExtractionService::Observer {
 public:
  // The priority to use when computing embeddings. Higher priorities imply more
  // performance overhead.
  enum Priority {
    kUserBlocking,
    kUrgent,
    kDefault,
    kBackground,
  };

  // The observer's requirements for using embeddings. Ordered by increasing
  // usage demands.
  enum UsageMode {
    // The embeddings are only required for one-off, on demand scenarios, based
    // on the currently open pages. In this mode embeddings are still computed
    // in the background for some WebContents, to avoid the latency and
    // performance impacts of performing embeddings computations across many
    // tabs when needed.
    kOnDemand,

    // The embeddings are required continouously for all page loads.
    kContinuous,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Gets the default priority to use for computing embeddings.
    // Implementations are expected to return the same value over the entire
    // lifetime of the observer.
    virtual Priority GetDefaultPriority() const;

    // Gets the usage mode for the observer, which indicates how it will use the
    // generated embeddings. Implementations are expected to return the same
    // value over the entire lifetime of the observer.
    virtual UsageMode GetUsageMode() const;

    // Invoked when embeddings become available or are updated for the
    // page. The embeddings then can be queried via GetEmbeddings().
    virtual void OnPageEmbeddingsAvailable(content::Page& page) {}
  };

  // ScopedPriority allows observers to temporarily raise the priority of the
  // embeddings computation for the lifetime of the object. This can be useful,
  // for example, if embeddings are anticipated to be needed urgently to drive
  // UI features.
  class ScopedPriority {
   public:
    ScopedPriority(PageEmbeddingsService* service,
                   Observer* observer,
                   Priority priority);
    ~ScopedPriority();

    ScopedPriority(ScopedPriority& other) = delete;
    ScopedPriority& operator=(ScopedPriority& other) = delete;

    ScopedPriority(ScopedPriority&& other);
    ScopedPriority& operator=(ScopedPriority&& other);

   private:
    raw_ptr<PageEmbeddingsService> service_;
    raw_ptr<PageEmbeddingsService::Observer> observer_;
  };

  // A callback to produce the passages for a page for which to generate
  // embeddings. This is responsible for generating chunked passages from the
  // AnnotatedPageContent and filtering to the top
  // `page_content_passages_to_generate` most useful passages.
  using EmbeddingCandidatesGenerator = base::RepeatingCallback<
      std::vector<std::pair<std::string, EmbeddingPassageType>>(
          const PageContent&,
          size_t page_content_passages_to_generate,
          const std::string& title,
          const std::string& url)>;

  PageEmbeddingsService(
      EmbeddingCandidatesGenerator candidates_generator,
      PageContentExtractionService* page_content_extraction_service,
      passage_embeddings::Embedder* embedder,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider);
  explicit PageEmbeddingsService(
      PageContentExtractionService* page_content_extraction_service);
  ~PageEmbeddingsService() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  ScopedPriority RaisePriority(Observer* observer, Priority priority);

  // In on demand mode PageEmbeddingsService computes embeddings lazily for the
  // active tab, on backgrounding. ProcessEmbeddingsOnDemand() forces the active
  // tab's embeddings to be processed for kOnDemand observers. Has no effect for
  // kContinuous observers. Virtual for testing.
  virtual void ProcessEmbeddingsOnDemand();

  // Retrieves the embeddings for page. Returns the empty vector if
  // embeddings have not yet been computed.
  // Virtual for testing.
  virtual std::vector<PassageEmbedding> GetEmbeddings(
      content::Page& page) const;

  // Returns the provider for embedder metadata.
  passage_embeddings::EmbedderMetadataProvider* GetEmbedderMetadataProvider();

  // PageContentExtractionService:
  void OnPageContentExtracted(content::Page& page,
                              PageContent page_content) override;

 private:
  class WebContentsEventsObserver;

  struct Pending;
  struct Computing;
  struct Available;
  struct Unavailable;

  struct WebContentsState;

  // Computes embeddings for the page. Expects that the embeddings state is
  // Pending, i.e. we have already received passages for the page.
  void ComputeEmbeddings(content::Page& page);
  void ComputeEmbeddingsOnHide(content::Page& page);

  void OnEmbeddingsComputed(
      std::vector<EmbeddingPassageType> passage_types,
      base::WeakPtr<content::WebContents> web_contents,
      base::WeakPtr<content::Page> page,
      std::vector<std::string> passage_strings,
      std::vector<passage_embeddings::Embedding> embeddings,
      uint64_t job_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  static Priority GetActivePriority(
      const base::ObserverList<Observer>& observers,
      const std::map<Observer*, Priority>& temporary_priority);

  void UpdateTaskPriorities(Priority priority);

  static UsageMode GetActiveUsageMode(
      const base::ObserverList<Observer>& observers);

  const EmbeddingCandidatesGenerator candidates_generator_;

  const raw_ptr<passage_embeddings::Embedder> embedder_;

  const raw_ptr<passage_embeddings::EmbedderMetadataProvider>
      embedder_metadata_provider_;

  raw_ptr<PageContentExtractionService> page_content_extraction_service_;
  base::ScopedObservation<PageContentExtractionService, PageEmbeddingsService>
      page_content_extraction_observation_{this};

  base::ObserverList<Observer> observers_;
  std::map<Observer*, Priority> temporary_priority_;

  Priority current_priority_ = kDefault;

  UsageMode current_usage_mode_ = kOnDemand;

  std::map<content::WebContents*, WebContentsState> web_contents_states_;

  base::WeakPtrFactory<PageEmbeddingsService> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_EMBEDDINGS_SERVICE_H_
