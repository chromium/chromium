// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/sync/service/sync_service.h"

class GURL;

namespace signin {
class IdentityManager;
}

namespace unified_consent {
class UrlKeyedDataCollectionConsentHelper;
}

namespace multistep_filter {

class AnnotationIndexClient;
class MultistepFilterLogRouter;
class MultistepFilterServiceTestApi;
class FilterExtractor;
class FilterStore;
class FilterSuggestionGenerator;

// Service to orchestrate Multistep Filter.
//
// Based on the user's past navigation history, this service stores and provides
// suggestions for filters to the user. It acts as the central
// coordinator for the Multistep Filter feature, managing the lifecycle of
// related components like the FilterSuggestionGenerator.
class MultistepFilterService : public KeyedService,
                               public history::HistoryServiceObserver {
 public:
  class ObserverForTest {
   public:
    virtual ~ObserverForTest() = default;
    virtual void OnExtractionFinished(
        std::optional<base::Uuid> annotation_id) = 0;
    virtual void OnSuggestionGenerated(
        std::optional<UrlFilterSuggestion> suggestion) = 0;
  };

  struct Params {
    std::unique_ptr<AnnotationIndexClient> annotation_index_client;
    std::unique_ptr<FilterStore> filter_store;
    raw_ptr<signin::IdentityManager> identity_manager;
    std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
        consent_helper;
    raw_ptr<MultistepFilterLogRouter> log_router;
    raw_ptr<history::HistoryService> history_service;
    raw_ptr<syncer::SyncService> sync_service;
  };

  explicit MultistepFilterService(Params params);

  MultistepFilterService(const MultistepFilterService&) = delete;
  MultistepFilterService& operator=(const MultistepFilterService&) = delete;

  ~MultistepFilterService() override;

  // KeyedService:
  void Shutdown() override;

  // Parses the given url to extract a `FilterAnnotation`. A filter annotation
  // is a set of normalized filter attributes.
  virtual void ExtractAnnotation(int64_t navigation_id, const GURL& url);

  // Generates a filter suggestion for `url`. Based on URL analysis, the
  // suggestion may be stored for later use. Results are returned via the
  // `callback`.
  virtual void GenerateFilterSuggestions(
      int64_t navigation_id,
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback);

  // Deletes all annotations for the given `task_type`.
  virtual void DeleteAnnotationsForTask(std::string_view task_type,
                                        int64_t navigation_id,
                                        std::string_view domain);

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class MultistepFilterServiceTestApi;

  // Callback for when an annotation is extracted.
  void OnExtractionFinished(std::optional<base::Uuid> annotation_id);

  // Callback for when a suggestion is generated.
  void OnSuggestionGenerated(
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
      std::optional<UrlFilterSuggestion> suggestion);

  // Checks if `url` is eligible under the current sign-in, consent, and sync
  // status, and logs the decision.
  bool IsUrlAllowed(const GURL& url,
                    int64_t navigation_id,
                    std::string_view domain);

  // Returns true if the user is currently signed in. The Multistep Filter
  // feature is only available for signed-in users.
  bool IsUserSignedIn() const;

  // Returns true if the user has enabled URL-keyed data collection.
  bool IsUrlKeyedDataCollectionEnabled() const;

  // Returns true if history sync is enabled.
  bool IsHistorySyncEnabled() const;

  raw_ptr<ObserverForTest> observer_for_test_ = nullptr;

  // Client used to interact with the `SiteAutomationIndexServer` on the server
  // side.
  std::unique_ptr<AnnotationIndexClient> annotation_index_client_;

  // Provides access to the underlying database that persists the user's
  // filter suggestions.
  std::unique_ptr<FilterStore> filter_store_;

  // Extracts filter annotations from URLs and stores them. Never null.
  std::unique_ptr<FilterExtractor> filter_extractor_;

  // Responsible for generating filter suggestions.
  std::unique_ptr<FilterSuggestionGenerator> filter_suggestion_generator_;

  // Used to check if the user is signed in, as the feature is only available
  // for signed-in users.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Used to check for URL-keyed data collection consent.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      consent_helper_;

  // Log router for the internals page.
  raw_ptr<MultistepFilterLogRouter> log_router_;

  // Sync service to check for history sync state.
  raw_ptr<syncer::SyncService> sync_service_;

  // History service observer to listen for history deletions.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<MultistepFilterService> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_
