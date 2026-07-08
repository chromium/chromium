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

  // Checks if the user has provided consent (signed in, URL-keyed data
  // collection enabled, and history sync enabled), and logs the eligibility
  // check.
  virtual bool HasUserProvidedConsent(int64_t navigation_id,
                                      std::string_view host);

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

  // Returns the FilterStore owned by this service.
  FilterStore* GetFilterStore() const { return filter_store_.get(); }

  // Returns the AnnotationIndexClient owned by this service.
  AnnotationIndexClient* GetAnnotationIndexClient() const {
    return annotation_index_client_.get();
  }

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class MultistepFilterServiceTestApi;

  // Returns true if the user is currently signed in. The Multistep Filter
  // feature is only available for signed-in users.
  bool IsUserSignedIn() const;

  // Returns true if the user has enabled URL-keyed data collection.
  bool IsUrlKeyedDataCollectionEnabled() const;

  // Returns true if history sync is enabled.
  bool IsHistorySyncEnabled() const;

  // Client used to interact with the `SiteAutomationIndexServer` on the server
  // side.
  std::unique_ptr<AnnotationIndexClient> annotation_index_client_;

  // Provides access to the underlying database that persists the user's
  // filter suggestions.
  std::unique_ptr<FilterStore> filter_store_;

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
