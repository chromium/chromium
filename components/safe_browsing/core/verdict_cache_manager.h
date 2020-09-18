// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_VERDICT_CACHE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_VERDICT_CACHE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "url/gurl.h"

class HostContentSettingsMap;

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

// Structure: http://screen/YaNfDRYrcnk.png.
class VerdictCacheManager : public history::HistoryServiceObserver,
                            public KeyedService {
 public:
  explicit VerdictCacheManager(
      history::HistoryService* history_service,
      scoped_refptr<HostContentSettingsMap> content_settings);
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

  // Stores |verdict| in |content_settings_| based on its |url|, |verdict| and
  // |receive_time|.
  // |store_old_cache| is used for compatibility test. It is set to true only
  // when we need to store an old cache for testing.
  // TODO(crbug.com/1049376): |store_old_cache| should be removed once
  // |cache_expression| field is deprecated.
  void CacheRealTimeUrlVerdict(const GURL& url,
                               const RTLookupResponse& verdict,
                               const base::Time& receive_time,
                               bool store_old_cache);

  // Looks up |content_settings_| to find the cached verdict response. If
  // verdict is not available or is expired, return VERDICT_TYPE_UNSPECIFIED.
  // Otherwise, the most matching theat info will be copied to out_threat_info.
  // Can be called on any thread.
  RTLookupResponse::ThreatInfo::VerdictType GetCachedRealTimeUrlVerdict(
      const GURL& url,
      RTLookupResponse::ThreatInfo* out_threat_info);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Returns true if an artificial unsafe URL has been provided using the
  // command-line flag "mark_as_real_time_phishing".
  static bool has_artificial_unsafe_url();

  void StopCleanUpTimerForTesting();

 private:
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

  void ScheduleNextCleanUpAfterInterval(base::TimeDelta interval);

  // Removes all the expired verdicts from cache.
  void CleanUpExpiredVerdicts();
  void CleanUpExpiredPhishGuardVerdicts();
  void CleanUpExpiredRealTimeUrlCheckVerdicts();

  // Helper method to remove content settings when URLs are deleted. If
  // |all_history| is true, removes all cached verdicts. Otherwise it removes
  // all verdicts associated with the deleted URLs in |deleted_rows|.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows);
  bool RemoveExpiredPhishGuardVerdicts(
      LoginReputationClientRequest::TriggerType trigger_type,
      base::Value* cache_dictionary);
  bool RemoveExpiredRealTimeUrlCheckVerdicts(base::Value* cache_dictionary);

  size_t GetPhishGuardVerdictCountForURL(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type);
  // This method is only used for testing.
  size_t GetRealTimeUrlCheckVerdictCountForURL(const GURL& url);
  // Gets the total number of RealTimeUrlCheck verdicts we cached
  // for this profile. This counts both expired and active verdicts.
  size_t GetStoredRealTimeUrlCheckVerdictCount();

  // This adds a cached verdict for a URL that has artificially been marked as
  // unsafe using the command line flag "mark_as_real_time_phishing".
  void CacheArtificialVerdict();

  // Number of verdict stored for this profile for password on focus pings.
  base::Optional<size_t> stored_verdict_count_password_on_focus_;

  // Number of verdict stored for this profile for protected password entry
  // pings.
  base::Optional<size_t> stored_verdict_count_password_entry_;

  // Number of verdict stored for this profile for real time url check pings.
  base::Optional<size_t> stored_verdict_count_real_time_url_check_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  // Content settings maps associated with this instance.
  scoped_refptr<HostContentSettingsMap> content_settings_;

  base::OneShotTimer cleanup_timer_;

  base::WeakPtrFactory<VerdictCacheManager> weak_factory_{this};

  static bool has_artificial_unsafe_url_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_VERDICT_CACHE_MANAGER_H_
