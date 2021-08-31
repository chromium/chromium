// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_hints_component_observer.h"
#include "components/optimization_guide/core/push_notification_manager.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class OptimizationGuideNavigationData;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
class NetworkConnectionTracker;
}  // namespace network

namespace optimization_guide {
class HintCache;
class HintsFetcherFactory;
class OptimizationFilter;
class OptimizationMetadata;
class OptimizationGuideStore;
enum class OptimizationTargetDecision;
enum class OptimizationTypeDecision;
class StoreUpdateData;
class TabUrlProvider;
class TopHostProvider;

class HintsManager : public OptimizationHintsComponentObserver,
                     public PushNotificationManager::Delegate {
 public:
  HintsManager(
      bool is_off_the_record,
      const std::string& application_locale,
      PrefService* pref_service,
      optimization_guide::OptimizationGuideStore* hint_store,
      optimization_guide::TopHostProvider* top_host_provider,
      optimization_guide::TabUrlProvider* tab_url_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<optimization_guide::PushNotificationManager>
          push_notification_manager);

  ~HintsManager() override;

  HintsManager(const HintsManager&) = delete;
  HintsManager& operator=(const HintsManager&) = delete;

  // Unhooks the observer to |optimization_guide_service_|.
  void Shutdown();

  // Returns the OptimizationGuideDecision from |optimization_type_decision|.
  static optimization_guide::OptimizationGuideDecision
  GetOptimizationGuideDecisionFromOptimizationTypeDecision(
      optimization_guide::OptimizationTypeDecision optimization_type_decision);

  // optimization_guide::OptimizationHintsComponentObserver implementation:
  void OnHintsComponentAvailable(
      const optimization_guide::HintsComponentInfo& info) override;

  // |next_update_closure| is called the next time OnHintsComponentAvailable()
  // is called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

  // Registers the optimization types that have the potential for hints to be
  // called by consumers of the Optimization Guide.
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Returns the optimization types that are registered.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() const {
    return registered_optimization_types_;
  }

  // Returns whether there is an optimization allowlist loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationAllowlist(
      optimization_guide::proto::OptimizationType optimization_type);
  // Returns whether there is an optimization blocklist loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationBlocklist(
      optimization_guide::proto::OptimizationType optimization_type);

  // Returns the OptimizationTypeDecision based on the given parameters.
  // |optimization_metadata| will be populated, if applicable.
  optimization_guide::OptimizationTypeDecision CanApplyOptimization(
      const GURL& navigation_url,
      const absl::optional<int64_t>& navigation_id,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata);

  // Invokes |callback| with the decision for |navigation_url| and
  // |optimization_type|, when sufficient information has been collected by
  // |this| to make the decision. Virtual for testing.
  virtual void CanApplyOptimizationAsync(
      const GURL& navigation_url,
      const absl::optional<int64_t>& navigation_id,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback);

  // Clears all fetched hints from |hint_cache_|.
  void ClearFetchedHints();

  // Clears the host-keyed fetched hints from |hint_cache_|, both the persisted
  // and in memory ones.
  void ClearHostKeyedHints();

  // Returns the current batch update hints fetcher.
  optimization_guide::HintsFetcher* batch_update_hints_fetcher() const {
    return batch_update_hints_fetcher_.get();
  }

  // Overrides |hints_fetcher_factory| for testing.
  void SetHintsFetcherFactoryForTesting(
      std::unique_ptr<optimization_guide::HintsFetcherFactory>
          hints_fetcher_factory);

  // Overrides |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Notifies |this| that a navigation with |navigation_data| started.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data,
      base::OnceClosure callback);

  // Notifies |this| that a navigation with redirect chain
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // Notifies |this| that deferred startup has occurred. This enables |this|
  // to execute background tasks while minimizing the risk of regressing
  // metrics such as jank.
  void OnDeferredStartup();

  // Fetch the hints for the given URLs.
  void FetchHintsForURLs(std::vector<GURL> target_urls);

  // optimization_guide::PushNotificationManager::Delegate:
  void RemoveFetchedEntriesByHintKeys(
      base::OnceClosure on_success,
      optimization_guide::proto::KeyRepresentation key_representation,
      const base::flat_set<std::string>& hint_keys) override;
  void PurgeFetchedEntries(base::OnceClosure on_success) override;

  // Returns true if |this| is allowed to fetch hints at the navigation time for
  // |url|.
  bool IsAllowedToFetchNavigationHints(const GURL& url);

  // Returns the hint cache for |this|.
  optimization_guide::HintCache* hint_cache();

  // Returns the persistent store for |this|.
  optimization_guide::OptimizationGuideStore* hint_store();

  // Returns the push notification manager for |this|. May be nullptr;
  optimization_guide::PushNotificationManager* push_notification_manager();

  // Add hints to the cache with the provided metadata. For testing only.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const absl::optional<optimization_guide::OptimizationMetadata>& metadata);

 private:
  // Processes the optimization filters contained in the hints component.
  void ProcessOptimizationFilters(
      const google::protobuf::RepeatedPtrField<
          optimization_guide::proto::OptimizationFilter>&
          allowlist_optimization_filters,
      const google::protobuf::RepeatedPtrField<
          optimization_guide::proto::OptimizationFilter>&
          blocklist_optimization_filters);

  // Process a set of optimization filters.
  //
  // |is_allowlist| will be used to ensure that the filters are either uses as
  // allowlists or blocklists.
  void ProcessOptimizationFilterSet(
      const google::protobuf::RepeatedPtrField<
          optimization_guide::proto::OptimizationFilter>& filters,
      bool is_allowlist);

  // Callback run after the hint cache is fully initialized. At this point,
  // the HintsManager is ready to process hints.
  void OnHintCacheInitialized();

  // Updates the cache with the latest hints sent by the Component Updater.
  void UpdateComponentHints(
      base::OnceClosure update_closure,
      std::unique_ptr<optimization_guide::StoreUpdateData> update_data,
      std::unique_ptr<optimization_guide::proto::Configuration> config);

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  void OnComponentHintsUpdated(base::OnceClosure update_closure,
                               bool hints_updated);

  // Initiates fetching of hints - either immediately over via a timer.
  void InitiateHintsFetchScheduling();

  // Returns the URLs that are currently in the active tab model that do not
  // have a hint available in |hint_cache_|.
  const std::vector<GURL> GetActiveTabURLsToRefresh();

  // Schedules |active_tabs_hints_fetch_timer_| to fire based on the last time a
  // fetch attempt was made.
  void ScheduleActiveTabsHintsFetch();

  // Called to make a request to fetch hints from the remote Optimization Guide
  // Service. Used to fetch hints for origins frequently visited by the user and
  // URLs open in the active tab model.
  void FetchHintsForActiveTabs();

  // Called when the hints for active tabs have been fetched from the remote
  // Optimization Guide Service and are ready for parsing. This is used when
  // fetching hints in batch mode.
  void OnHintsForActiveTabsFetched(
      const base::flat_set<std::string>& hosts_fetched,
      const base::flat_set<GURL>& urls_fetched,
      absl::optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response);

  // Called when the hints for a navigation have been fetched from the remote
  // Optimization Guide Service and are ready for parsing. This is used when
  // fetching hints in real-time. |navigation_url| is the URL associated with
  // the navigation handle that initiated the fetch.
  // |page_navigation_urls_requested| contains the URLs that were requested  by
  // |this| to be fetched. |page_navigation_hosts_requested| contains the hosts
  // that were requested by |this| to be fetched.
  void OnPageNavigationHintsFetched(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
      const absl::optional<GURL>& navigation_url,
      const base::flat_set<GURL>& page_navigation_urls_requested,
      const base::flat_set<std::string>& page_navigation_hosts_requested,
      absl::optional<
          std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
          get_hints_response);

  // Called when the fetched hints have been stored in |hint_cache| and are
  // ready to be used. This is used when hints were fetched in batch mode.
  void OnFetchedActiveTabsHintsStored();

  // Called when the fetched hints have been stored in |hint_cache| and are
  // ready to be used. This is used when hints were fetched in real-time.
  // |navigation_url| is the URL associated with the navigation handle that
  // initiated the fetch. |page_navigation_hosts_requested| contains the hosts
  // whose hints should be loaded into memory when invoked.
  void OnFetchedPageNavigationHintsStored(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data_weak_ptr,
      const absl::optional<GURL>& navigation_url,
      const base::flat_set<std::string>& page_navigation_hosts_requested);

  // Returns true if there is a fetch currently in-flight for |navigation_url|.
  bool IsHintBeingFetchedForNavigation(const GURL& navigation_url);

  // Cleans up the hints fetcher for |navigation_url|, if applicable.
  void CleanUpFetcherForNavigation(const GURL& navigation_url);

  // Returns the time when a hints fetch request was last attempted.
  base::Time GetLastHintsFetchAttemptTime() const;

  // Sets the time when a hints fetch was last attempted to |last_attempt_time|.
  void SetLastHintsFetchAttemptTime(base::Time last_attempt_time);

  // Called when the request to load a hint has completed.
  void OnHintLoaded(base::OnceClosure callback,
                    const optimization_guide::proto::Hint* loaded_hint) const;

  // Loads the hint if available for navigation to |url|.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void LoadHintForURL(const GURL& url, base::OnceClosure callback);

  // Loads the hint for |host| if available.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that |host| or not. The callback can be used as a
  // signal for tests.
  void LoadHintForHost(const std::string& host, base::OnceClosure callback);

  // Returns whether there is an optimization type to fetch for. Will return
  // false if no optimization types are registered or if all registered
  // optimization types are covered by optimization filters.
  bool HasOptimizationTypeToFetchFor();

  // Creates a hints fetch for navigation represented by |navigation_data|, if
  // it is allowed. The fetch will include the host and URL of the
  // |navigation_data| if the associated hints for each are not already in the
  // cache.
  void MaybeFetchHintsForNavigation(
      OptimizationGuideNavigationData* navigation_data);

  // If an entry for |navigation_url| is contained in |registered_callbacks_|,
  // it will load the hint for |navigation_url|'s host and upon completion, will
  // invoke the registered callbacks for |navigation_url|.
  void PrepareToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Invokes the registered callbacks for |navigation_url|, if applicable.
  void OnReadyToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Whether all information was available to make a decision for
  // |navigation_url| and |optimization type}.
  bool HasAllInformationForDecisionAvailable(
      const GURL& navigation_url,
      optimization_guide::proto::OptimizationType optimization_type);

  // The information of the latest component delivered by
  // |optimization_guide_service_|.
  absl::optional<optimization_guide::HintsComponentInfo> hints_component_info_;

  // Whether the component is currently being processed.
  bool is_processing_component_ = false;

  // The set of optimization types that have been registered with the hints
  // manager.
  //
  // Should only be read and modified on the UI thread.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // The set of optimization types that the component specified by
  // |component_info_| has optimization filters for.
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types_with_filter_;

  // A map from optimization type to the host filter that holds the allowlist
  // for that type.
  base::flat_map<optimization_guide::proto::OptimizationType,
                 std::unique_ptr<optimization_guide::OptimizationFilter>>
      allowlist_optimization_filters_;

  // A map from optimization type to the host filter that holds the blocklist
  // for that type.
  base::flat_map<optimization_guide::proto::OptimizationType,
                 std::unique_ptr<optimization_guide::OptimizationFilter>>
      blocklist_optimization_filters_;

  // A map from URL to a map of callbacks (along with the navigation IDs that
  // they were called for) keyed by their optimization type.
  base::flat_map<
      GURL,
      base::flat_map<
          optimization_guide::proto::OptimizationType,
          std::vector<std::pair<
              absl::optional<int64_t>,
              optimization_guide::OptimizationGuideDecisionCallback>>>>
      registered_callbacks_;

  // Whether |this| was created for an off the record profile.
  const bool is_off_the_record_;

  // The current applcation locale of Chrome.
  const std::string application_locale_;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // The hint cache that holds both hints received from the component and
  // fetched from the remote Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintCache> hint_cache_;

  // The fetcher that handles making requests for hints for multiple hosts from
  // the remote Optimization Guide Service.
  std::unique_ptr<optimization_guide::HintsFetcher> batch_update_hints_fetcher_;

  // A cache keyed by navigation URL to the fetcher making a request for a hint
  // for that URL and/or host to the remote Optimization Guide Service that
  // keeps track of when an entry has been placed in the cache.
  base::MRUCache<GURL, std::unique_ptr<optimization_guide::HintsFetcher>>
      page_navigation_hints_fetchers_;

  // The factory used to create hints fetchers. It is mostly used to create
  // new fetchers for use under the page navigation context, but will also be
  // used to create the initial fetcher for the batch update context.
  std::unique_ptr<optimization_guide::HintsFetcherFactory>
      hints_fetcher_factory_;

  // The top host provider that can be queried. Not owned.
  optimization_guide::TopHostProvider* top_host_provider_ = nullptr;

  // The tab URL provider that can be queried. Not owned.
  optimization_guide::TabUrlProvider* tab_url_provider_ = nullptr;

  // The timer used to schedule fetching hints from the remote Optimization
  // Guide Service.
  base::OneShotTimer active_tabs_hints_fetch_timer_;

  // The class that handles push notification processing and informs |this| of
  // what to do through the implemented Delegate above.
  std::unique_ptr<optimization_guide::PushNotificationManager>
      push_notification_manager_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  const base::Clock* clock_;

  // Whether fetched hints should be cleared when the store is initialized
  // because a new optimization type was registered.
  bool should_clear_hints_for_new_type_ = false;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // Background thread where hints processing should be performed.
  //
  // Warning: This must be the last object, so it is destroyed (and flushed)
  // first. This will prevent use-after-free issues where the background thread
  // would access other member variables after they have been destroyed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self.
  base::WeakPtrFactory<HintsManager> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_MANAGER_H_
