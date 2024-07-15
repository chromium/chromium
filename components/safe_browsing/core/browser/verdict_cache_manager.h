// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_VERDICT_CACHE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_VERDICT_CACHE_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_cache.h"
#include "components/safe_browsing/core/browser/safe_browsing_sync_observer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class SafeBrowsingServiceTest;

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

// Structure: http://screen/YaNfDRYrcnk.png.
class VerdictCacheManager : public history::HistoryServiceObserver,
                            public KeyedService {
 public:
  VerdictCacheManager(history::HistoryService* history_service,
                      scoped_refptr<HostContentSettingsMap> content_settings,
                      PrefService* pref_service,
                      std::unique_ptr<SafeBrowsingSyncObserver> sync_observer);
  VerdictCacheManager(const VerdictCacheManager&) = delete;
  VerdictCacheManager& operator=(const VerdictCacheManager&) = delete;
  VerdictCacheManager(VerdictCacheManager&&) = delete;
  VerdictCacheManager& operator=(const VerdictCacheManager&&) = delete;

  ~VerdictCacheManager() override;

  base::WeakPtr<VerdictCacheManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  // Stores |verdict| in |content_settings_| based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  void CachePhishGuardVerdict(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      const LoginReputationClientResponse& verdict,
      const base::Time& receive_time);

  // Looks up |content_settings_| to find the cached verdict response. If
  // verdict is not available or is expired, return VERDICT_TYPE_UNSPECIFIED.
  // Can be called on any thread.
  LoginReputationClientResponse::VerdictType GetCachedPhishGuardVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response);

  // Gets the total number of verdicts of the specified |trigger_type| we cached
  // for this profile. This counts both expired and active verdicts.
  size_t GetStoredPhishGuardVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type);

  // Stores |verdict| in |content_settings_| based on its |verdict| and
  // |receive_time|.
  void CacheRealTimeUrlVerdict(const RTLookupResponse& verdict,
                               const base::Time& receive_time);

  // Looks up |content_settings_| to find the cached verdict response. If
  // verdict is not available or is expired, return VERDICT_TYPE_UNSPECIFIED.
  // Otherwise, the most matching theat info will be copied to out_threat_info.
  // Can be called on any thread.
  RTLookupResponse::ThreatInfo::VerdictType GetCachedRealTimeUrlVerdict(
      const GURL& url,
      RTLookupResponse::ThreatInfo* out_threat_info);

  safe_browsing::ClientSideDetectionType
  GetCachedRealTimeUrlClientSideDetectionType(const GURL& url);

  // Creates a page load token that is tied with the hostname of the |url|.
  // The token is stored in memory.
  ChromeUserPopulation::PageLoadToken CreatePageLoadToken(const GURL& url);

  // Gets the page load token for the hostname of the |url|. Returns an empty
  // token if the token is not found.
  ChromeUserPopulation::PageLoadToken GetPageLoadToken(const GURL& url);

  // Stores the results of a hash-prefix real-time lookup into a cache object.
  void CacheHashPrefixRealTimeLookupResults(
      const std::vector<std::string>& requested_hash_prefixes,
      const std::vector<V5::FullHash>& response_full_hashes,
      const V5::Duration& cache_duration);

  // Searches the hash-prefix real-time cache object for the requested
  // |hash_prefixes|.
  std::unordered_map<std::string, std::vector<V5::FullHash>>
  GetCachedHashPrefixRealTimeLookupResults(
      const std::set<std::string>& hash_prefixes);

  // Overridden from history::HistoryServiceObserver.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Called by browsing data remover.
  void OnCookiesDeleted();

  // Returns true if an artificial URL has been provided either using
  // command-line flags or through a test.
  static bool has_artificial_cached_url();

  void StopCleanUpTimerForTesting();
  void SetPageLoadTokenForTesting(const GURL& url,
                                  ChromeUserPopulation::PageLoadToken token);

 private:
  friend class ::SafeBrowsingServiceTest;
  friend class SafeBrowsingBlockingPageAsyncChecksTestBase;
  friend class SafeBrowsingBlockingPageRealTimeUrlCheckTest;
  friend class SafeBrowsingBlockingPageHashRealTimeCheckTest;
  friend class VerdictCacheManagerTest;
  friend class ArtificialHashRealTimeVerdictCacheManagerTest;
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest, TestCleanUpExpiredVerdict);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestCleanUpExpiredVerdictWithInvalidEntry);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestRemoveCachedVerdictOnURLsDeleted);
  FRIEND_TEST_ALL_PREFIXES(
      VerdictCacheManagerTest,
      TestRemoveRealTimeUrlCheckCachedVerdictOnURLsDeleted);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestCleanUpExpiredVerdictInBackground);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestCleanUpVerdictOlderThanUpperBound);
  FRIEND_TEST_ALL_PREFIXES(ArtificialHashRealTimeVerdictCacheManagerTest,
                           TestCachePopulated);

  // Enum representing the reason why page load tokens are cleared. Used to log
  // histograms. Entries must not be removed or reordered.
  enum class ClearReason {
    kSafeBrowsingStateChanged = 0,
    kCookiesDeleted = 1,
    kSyncStateChanged = 2,

    kMaxValue = kSyncStateChanged
  };

  void ScheduleNextCleanUpAfterInterval(base::TimeDelta interval);

  // Removes all the expired verdicts from cache.
  void CleanUpExpiredVerdicts();
  void CleanUpExpiredPhishGuardVerdicts();
  void CleanUpExpiredRealTimeUrlCheckVerdicts();
  void CleanUpExpiredPageLoadTokens();
  void CleanUpAllPageLoadTokens(ClearReason reason);
  void CleanUpExpiredHashPrefixRealTimeLookupResults();

  // Helper method to remove content settings when URLs are deleted. If
  // |all_history| is true, removes all cached verdicts. Otherwise it removes
  // all verdicts associated with the deleted URLs in |deleted_rows|.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows);
  bool RemoveExpiredPhishGuardVerdicts(
      LoginReputationClientRequest::TriggerType trigger_type,
      base::Value::Dict& cache_dictionary);
  bool RemoveExpiredRealTimeUrlCheckVerdicts(
      base::Value::Dict& cache_dictionary);

  size_t GetPhishGuardVerdictCountForURL(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type);
  // This method is only used for testing.
  size_t GetRealTimeUrlCheckVerdictCountForURL(const GURL& url);
  // Gets the total number of RealTimeUrlCheck verdicts we cached
  // for this profile. This counts both expired and active verdicts.
  size_t GetStoredRealTimeUrlCheckVerdictCount();

  // This adds a cached verdict for a URL that has artificially been marked as
  // unsafe using the command line flag "mark_as_real_time_phishing". This
  // applies to URL real-time lookups.
  void CacheArtificialUnsafeRealTimeUrlVerdictFromSwitch();

  // This adds a cached verdict for a URL that has artificially been marked as
  // safe or unsafe (depending on |is_unsafe|). This applies to URL real-time
  // lookups.
  void CacheArtificialRealTimeUrlVerdict(const std::string& url_string,
                                         bool is_unsafe);

  // This adds a cached verdict for a URL that has artificially been marked as
  // unsafe using the command line flag "mark_as_phish_guard_phishing". This
  // applies to Phishguard pings.
  void CacheArtificialUnsafePhishGuardVerdictFromSwitch();

  // This adds a cached verdict for a URL that has artificially been marked as
  // unsafe using the command line flag
  // "mark_as_hash_prefix_real_time_phishing". This applies to hash-prefix
  // real-time lookups.
  void CacheArtificialUnsafeHashRealTimeLookupVerdictFromSwitch();

  // This adds a cached verdict for a URL that has artificially been marked as
  // safe or unsafe (depending on |is_unsafe|). This applies to hash-prefix
  // real-time lookups.
  void CacheArtificialHashRealTimeLookupVerdict(const std::string& url_spec,
                                                bool is_unsafe);

  // Resets the value of |has_artificial_cached_url_| back to false. If a unit
  // test sets an artificial URL, it is responsible for resetting the value
  // when the test completes so that it's not still true when later unit tests
  // run.
  static void ResetHasArtificialCachedUrlForTesting();

  // Number of verdict stored for this profile for password on focus pings.
  std::optional<size_t> stored_verdict_count_password_on_focus_;

  // Number of verdict stored for this profile for protected password entry
  // pings.
  std::optional<size_t> stored_verdict_count_password_entry_;

  // Number of verdict stored for this profile for real time url check pings.
  std::optional<size_t> stored_verdict_count_real_time_url_check_;

  // A map of page load tokens, keyed by the hostname.
  base::flat_map<std::string, ChromeUserPopulation::PageLoadToken>
      page_load_token_map_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Content settings maps associated with this instance.
  scoped_refptr<HostContentSettingsMap> content_settings_;

  base::OneShotTimer cleanup_timer_;

  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<SafeBrowsingSyncObserver> sync_observer_;

  // The local cache object for hash-prefix real-time lookups.
  std::unique_ptr<HashRealTimeCache> hash_realtime_cache_ =
      std::make_unique<HashRealTimeCache>();

  bool is_shut_down_ = false;

  base::WeakPtrFactory<VerdictCacheManager> weak_factory_{this};

  static bool has_artificial_cached_url_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_VERDICT_CACHE_MANAGER_H_
