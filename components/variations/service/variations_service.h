// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_request_scheduler.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/variations/variations_seed_store.h"
#include "components/version_info/version_info.h"
#include "components/web_resource/resource_request_allowed_notifier.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class FeatureList;
class Version;
}

namespace metrics {
class MetricsStateManager;
}

namespace network {
class SimpleURLLoader;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
class VariationsSeed;
}

namespace variations {

// If enabled, seed fetches will be retried over HTTP after an HTTPS request
// fails.
extern const base::Feature kHttpRetryFeature;

// Used to setup field trials based on stored variations seed data, and fetch
// new seed data from the variations server.
class VariationsService
    : public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  class Observer {
   public:
    // How critical a detected experiment change is. Whether it should be
    // handled on a "best-effort" basis or, for a more critical change, if it
    // should be given higher priority.
    enum Severity {
      BEST_EFFORT,
      CRITICAL,
    };

    // Called when the VariationsService detects that there will be significant
    // experiment changes on a restart. This notification can then be used to
    // update UI (i.e. badging an icon).
    virtual void OnExperimentChangesDetected(Severity severity) = 0;

   protected:
    virtual ~Observer() {}
  };

  ~VariationsService() override;

  // Enum used to choose whether GetVariationsServerURL will return an HTTPS
  // URL or an HTTP one. The HTTP URL is used as a fallback for seed retrieval
  // in cases where an HTTPS connection fails.
  enum HttpOptions { USE_HTTP, USE_HTTPS };

  // Should be called before startup of the main message loop.
  void PerformPreMainMessageLoopStartup();

  // Adds an observer to listen for detected experiment changes.
  void AddObserver(Observer* observer);

  // Removes a previously-added observer.
  void RemoveObserver(Observer* observer);

  // Called when the application enters foreground. This may trigger a
  // FetchVariationsSeed call.
  // TODO(rkaplow): Handle this and the similar event in metrics_service by
  // observing an 'OnAppEnterForeground' event instead of requiring the frontend
  // code to notify each service individually.
  void OnAppEnterForeground();

  // Sets the value of the "restrict" URL param to the variations service that
  // should be used for variation seed requests. This takes precedence over any
  // value coming from policy prefs. This should be called prior to any calls
  // to |StartRepeatedVariationsSeedFetch|.
  void SetRestrictMode(const std::string& restrict_mode);

  // Returns the variations server URL. |http_options| determines whether to
  // use the http or https URL. This function will return an empty GURL when
  // the restrict param exists for USE_HTTP, to indicate that no HTTP fallback
  // should happen in that case.
  GURL GetVariationsServerURL(HttpOptions http_options);

  // Returns the permanent overridden country code stored for this client. This
  // value will not be updated on Chrome updates.
  std::string GetOverriddenPermanentCountry();

  // Returns the permanent country code stored for this client. Country code is
  // in the format of lowercase ISO 3166-1 alpha-2. Example: us, br, in
  std::string GetStoredPermanentCountry();

  // Forces an override of the stored permanent country. Returns true
  // if the variable has been updated. Return false if the override country is
  // the same as the stored variable, or if the update failed for any other
  // reason.
  bool OverrideStoredPermanentCountry(const std::string& override_country);

  // Returns what variations will consider to be the latest country. Returns
  // empty if it is not available.
  std::string GetLatestCountry() const;

  // Ensures the locale that was used for evaluating variations matches the
  // passed |locale|. This is used to ensure that the locale determined after
  // loading the resource bundle (which is passed here) corresponds to what
  // was used for variations during an earlier stage of start up.
  void EnsureLocaleEquals(const std::string& locale);

  // Exposed for testing.
  static std::string GetDefaultVariationsServerURLForTesting();

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Register Variations related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Factory method for creating a VariationsService. Does not take ownership of
  // |state_manager|. Caller should ensure that |state_manager| is valid for the
  // lifetime of this class.
  static std::unique_ptr<VariationsService> Create(
      std::unique_ptr<VariationsServiceClient> client,
      PrefService* local_state,
      metrics::MetricsStateManager* state_manager,
      const char* disable_network_switch,
      const UIStringOverrider& ui_string_overrider,
      web_resource::ResourceRequestAllowedNotifier::
          NetworkConnectionTrackerGetter network_connection_tracker_getter);

  // Enables fetching the seed for testing, even for unofficial builds. This
  // should be used along with overriding |DoActualFetch| or using
  // |net::TestURLLoaderFactory|.
  static void EnableFetchForTesting();

  // Set the PrefService responsible for getting policy-related preferences,
  // such as the restrict parameter.
  void set_policy_pref_service(PrefService* service) {
    DCHECK(service);
    policy_pref_service_ = service;
  }

  // Exposed for testing.
  void GetClientFilterableStateForVersionCalledForTesting();

  web_resource::ResourceRequestAllowedNotifier*
  GetResourceRequestAllowedNotifierForTesting() {
    return resource_request_allowed_notifier_.get();
  }

  // Wrapper around VariationsFieldTrialCreator::SetupFieldTrials().
  bool SetupFieldTrials(
      const char* kEnableGpuBenchmarking,
      const char* kEnableFeatures,
      const char* kDisableFeatures,
      const std::set<std::string>& unforceable_field_trials,
      const std::vector<std::string>& variation_ids,
      const std::vector<base::FeatureList::FeatureOverrideInfo>&
          extra_overrides,
      std::unique_ptr<base::FeatureList> feature_list,
      variations::PlatformFieldTrials* platform_field_trials);

  // Overrides cached UI strings on the resource bundle once it is initialized.
  void OverrideCachedUIStrings();

  int request_count() const { return request_count_; }

  // Cancels the currently pending fetch request.
  void CancelCurrentRequestForTesting();

  // Exposes StartRepeatedVariationsSeedFetch for testing.
  void StartRepeatedVariationsSeedFetchForTesting();

 protected:
  // Starts the fetching process once, where |OnURLFetchComplete| is called with
  // the response. This calls DoFetchToURL with the set url.
  virtual void DoActualFetch();

  // Attempts a seed fetch from the set |url|. Returns true if the fetch was
  // started successfully, false otherwise. |is_http_retry| should be true if
  // this is a retry over HTTP, false otherwise.
  virtual bool DoFetchFromURL(const GURL& url, bool is_http_retry);

  // Stores the seed to prefs. Set as virtual and protected so that it can be
  // overridden by tests.
  virtual bool StoreSeed(const std::string& seed_data,
                         const std::string& seed_signature,
                         const std::string& country_code,
                         base::Time date_fetched,
                         bool is_delta_compressed,
                         bool is_gzip_compressed,
                         bool fetched_insecurely);

  // Create an entropy provider based on low entropy. This is used to create
  // trials for studies that should only depend on low entropy, such as studies
  // that send experiment IDs to Google web properties. Virtual for testing.
  virtual std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateLowEntropyProvider();

  // Creates the VariationsService with the given |local_state| prefs service
  // and |state_manager|. Does not take ownership of |state_manager|. Caller
  // should ensure that |state_manager| is valid for the lifetime of this class.
  // Use the |Create| factory method to create a VariationsService.
  VariationsService(
      std::unique_ptr<VariationsServiceClient> client,
      std::unique_ptr<web_resource::ResourceRequestAllowedNotifier> notifier,
      PrefService* local_state,
      metrics::MetricsStateManager* state_manager,
      const UIStringOverrider& ui_string_overrider);

  // Sets the URL for querying the variations server. Used for testing.
  void set_variations_server_url(const GURL& url) {
    variations_server_url_ = url;
  }

  // Sets the URL for querying the variations server when doing HTTP retries.
  // Used for testing.
  void set_insecure_variations_server_url(const GURL& url) {
    insecure_variations_server_url_ = url;
  }

  // Sets the |last_request_was_http_retry_| flag. Used for testing.
  void set_last_request_was_http_retry(bool was_http_retry) {
    last_request_was_http_retry_ = was_http_retry;
  }

  // The client that provides access to the embedder's environment.
  // Protected so testing subclasses can access it.
  VariationsServiceClient* client() { return client_.get(); }

  // Exposes MaybeRetryOverHTTP for testing.
  bool CallMaybeRetryOverHTTPForTesting();

  // Records a successful fetch:
  //   (1) Resets failure streaks for Safe Mode.
  //   (2) Records the time of this fetch as the most recent successful fetch.
  // Protected so testing subclasses can call it.
  void RecordSuccessfulFetch();

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, Observer);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedStoredWhenOKStatus);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedNotStoredWhenNonOKStatus);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, InstanceManipulations);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           LoadPermanentConsistencyCountry);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, CountryHeader);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, GetVariationsServerURL);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, VariationsURLHasParams);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, RequestsInitiallyAllowed);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, RequestsInitiallyNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           SafeMode_SuccessfulFetchClearsFailureStreaks);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           SafeMode_NotModifiedFetchClearsFailureStreaks);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, InsecurelyFetchedSetWhenHTTP);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           InsecurelyFetchedNotSetWhenHTTPS);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, DoNotRetryAfterARetry);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           DoNotRetryIfInsecureURLIsHTTPS);

  void InitResourceRequestedAllowedNotifier();

  // Calls FetchVariationsSeed once and repeats this periodically. See
  // implementation for details on the period.
  void StartRepeatedVariationsSeedFetch();

  // Checks if prerequisites for fetching the Variations seed are met, and if
  // so, performs the actual fetch using |DoActualFetch|.
  void FetchVariationsSeed();

  // Notify any observers of this service based on the simulation |result|.
  void NotifyObservers(
      const variations::VariationsSeedSimulator::Result& result);

  // Called by SimpleURLLoader when |pending_seed_request_| load completes.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called by SimpleURLLoader when |pending_seed_request_| load is redirected.
  void OnSimpleLoaderRedirect(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHead& response_head,
      std::vector<std::string>* to_be_removed_headers);

  // Handles post-fetch events.
  void OnSimpleLoaderCompleteOrRedirect(
      std::unique_ptr<std::string> response_body,
      bool was_redirect);

  // Retry the fetch over HTTP, called by OnSimpleLoaderComplete when a request
  // fails. Returns true is the fetch was successfully started, this does not
  // imply the actual fetch was successful.
  bool MaybeRetryOverHTTP();

  // ResourceRequestAllowedNotifier::Observer implementation:
  void OnResourceRequestsAllowed() override;

  // Performs a variations seed simulation with the given |seed| and |version|
  // and logs the simulation results as histograms.
  void PerformSimulationWithVersion(
      std::unique_ptr<variations::VariationsSeed> seed,
      const base::Version& version);

  // Encrypts a string using the encrypted_messages component, input is passed
  // in as |plaintext|, outputs a serialized EncryptedMessage protobuf as
  // |encrypted|. Returns true on success, false on failure. The encryption can
  // be done in-place.
  bool EncryptString(const std::string& plaintext, std::string* encrypted);

  // Loads the country code to use for filtering permanent consistency studies,
  // updating the stored country code if the stored value was for a different
  // Chrome version. The country used for permanent consistency studies is kept
  // consistent between Chrome upgrades in order to avoid annoying the user due
  // to experiment churn while traveling.
  std::string LoadPermanentConsistencyCountry(
      const base::Version& version,
      const std::string& latest_country);

  std::unique_ptr<VariationsServiceClient> client_;

  // The pref service used to store persist the variations seed.
  PrefService* local_state_;

  // Used for instantiating entropy providers for variations seed simulation.
  // Weak pointer.
  metrics::MetricsStateManager* state_manager_;

  // Used to obtain policy-related preferences. Depending on the platform, will
  // either be Local State or Profile prefs.
  PrefService* policy_pref_service_;

  // Contains the scheduler instance that handles timing for requests to the
  // server. Initially NULL and instantiated when the initial fetch is
  // requested.
  std::unique_ptr<VariationsRequestScheduler> request_scheduler_;

  // Contains the current seed request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete|.
  std::unique_ptr<network::SimpleURLLoader> pending_seed_request_;

  // The value of the "restrict" URL param to the variations server that has
  // been specified via |SetRestrictMode|. If empty, the URL param will be set
  // based on policy prefs.
  std::string restrict_mode_;

  // The URL to use for querying the variations server.
  GURL variations_server_url_;

  // HTTP URL used as a fallback if HTTPS fetches fail. If not set, retries
  // over HTTP will not be attempted.
  GURL insecure_variations_server_url_;

  // Tracks whether the initial request to the variations server had completed.
  bool initial_request_completed_;

  // Indicates that the next request to the variations service shouldn't specify
  // that it supports delta compression. Set to true when a delta compressed
  // response encountered an error.
  bool disable_deltas_for_next_request_;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  std::unique_ptr<web_resource::ResourceRequestAllowedNotifier>
      resource_request_allowed_notifier_;

  // The start time of the last seed request. This is used to measure the
  // latency of seed requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // The number of requests to the variations server that have been performed.
  int request_count_;

  // List of observers of the VariationsService.
  base::ObserverList<Observer>::Unchecked observer_list_;

  // The main entry point for managing safe mode state.
  SafeSeedManager safe_seed_manager_;

  // Member responsible for creating trials from a variations seed.
  VariationsFieldTrialCreator field_trial_creator_;

  // True if the last request was a retry over http.
  bool last_request_was_http_retry_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VariationsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VariationsService);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_H_
