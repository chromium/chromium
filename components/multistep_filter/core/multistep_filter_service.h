// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/sync/service/sync_service.h"

class GURL;
class PrefService;

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
//
// One instance of `MultistepFilterService` is created per `BrowserContext`
// (i.e. per profile) and owned by the `BrowserContext`.
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
    raw_ptr<PrefService> pref_service;
    raw_ptr<syncer::SyncService> sync_service;
  };

  explicit MultistepFilterService(Params params);

  MultistepFilterService(const MultistepFilterService&) = delete;
  MultistepFilterService& operator=(const MultistepFilterService&) = delete;

  ~MultistepFilterService() override;

  // KeyedService:
  void Shutdown() override;

  // Parses the given url to extract a `FilterAnnotation`. A filter annotation
  // is a set of normalized filter attributes. The eventual extraction is
  // delegated to `filter_extractor_` which takes care of storing the results.
  // `ExtractAnnotation` is guaranteed to call `OnExtractionFinished`. The
  // `annotation` will be nullopt in case of failures.
  virtual void ExtractAnnotation(
      int64_t navigation_id,
      const GURL& url,
      std::optional<UrlFilterSuggestion> applied_suggestion);

  // Called when a navigation happened but the landing page has an insecure
  // scheme or is an error page (i.e. no annotation can be extracted).
  virtual void NetworkStatusPreventedExtraction(
      int64_t navigation_id,
      const GURL& url,
      std::optional<UrlFilterSuggestion> applied_suggestion,
      bool is_unsupported_scheme,
      int net_error_code,
      int http_response_code);

  // Checks if the user has provided consent (signed in, URL-keyed data
  // collection enabled, and history sync enabled), and logs the eligibility
  // check.
  virtual bool HasUserProvidedConsent(int64_t navigation_id,
                                      std::string_view host);

  // Generates a filter suggestion for `url`. Based on URL analysis, the
  // suggestion may be stored for later use. Results are returned via the
  // `callback`. `GenerateFilterSuggestions` is guaranteed to call
  // `OnSuggestionGenerated`. The `annotation` will be nullopt in case of
  // failures.
  virtual void GenerateFilterSuggestions(
      int64_t navigation_id,
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback);

  // Records a suggestion impression in Profile retention preferences.
  virtual void RecordSuggestionImpression();

  // Records a user interaction with a suggestion in Profile retention
  // preferences.
  virtual void RecordUserInteractionWithSuggestion(
      SuggestionUserDecision decision);

  // Deletes all annotations for the given `task_type`.
  virtual void DeleteAnnotationsForTask(std::string_view task_type,
                                        int64_t navigation_id,
                                        std::string_view host);

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class MultistepFilterServiceTestApi;

  // Callback for when `GetSupportedTaskForUrl` finishes for extraction.
  void OnUrlAllowedForExtraction(
      int64_t navigation_id,
      const GURL& url,
      std::optional<UrlFilterSuggestion> applied_suggestion,
      std::vector<std::string> supported_task_types);

  // Callback for when an annotation is extracted or failed. In case of failure,
  // `annotation` will be nullopt.
  void OnExtractionFinished(
      int64_t navigation_id,
      std::string host,
      std::optional<UrlFilterSuggestion> applied_suggestion,
      std::optional<FilterAnnotation> annotation);

  // Callback for when `GetSupportedTaskForUrl` finishes for suggestion
  // generation.
  void OnUrlAllowedForSuggestion(
      int64_t navigation_id,
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
      std::vector<std::string> supported_task_types);

  // Callback for when a suggestion is generated.
  void OnSuggestionGenerated(
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback,
      std::optional<UrlFilterSuggestion> suggestion);

  // Asynchronously retrieves the supported task types for `url` via the
  // annotation index client and returns them via `callback`.
  void GetSupportedTaskForUrl(
      int64_t navigation_id,
      const GURL& url,
      base::OnceCallback<void(std::vector<std::string>)> callback);

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

  // Pref service to record retention statistics.
  raw_ptr<PrefService> pref_service_;

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
