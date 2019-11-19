// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_VERDICT_CACHE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_VERDICT_CACHE_MANAGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "url/gurl.h"

class HostContentSettingsMap;

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

class VerdictCacheManager : public history::HistoryServiceObserver {
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

  // Stores |verdict| in |settings| based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  void CachePhishGuardVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      const LoginReputationClientResponse& verdict,
      const base::Time& receive_time);

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  LoginReputationClientResponse::VerdictType GetCachedPhishGuardVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response);

  // Gets the total number of verdicts of the specified |trigger_type| we cached
  // for this profile. This counts both expired and active verdicts.
  size_t GetStoredPhishGuardVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest, TestCleanUpExpiredVerdict);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestCleanUpExpiredVerdictWithInvalidEntry);
  FRIEND_TEST_ALL_PREFIXES(VerdictCacheManagerTest,
                           TestRemoveCachedVerdictOnURLsDeleted);

  // Removes all the expired verdicts from cache.
  void CleanUpExpiredVerdicts();

  // Helper method to remove content settings when URLs are deleted. If
  // |all_history| is true, removes all cached verdicts. Otherwise it removes
  // all verdicts associated with the deleted URLs in |deleted_rows|.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows);

  bool RemoveExpiredVerdicts(
      LoginReputationClientRequest::TriggerType trigger_type,
      base::DictionaryValue* cache_dictionary);

  size_t GetVerdictCountForURL(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type);

  // Number of verdict stored for this profile for password on focus pings.
  base::Optional<size_t> stored_verdict_count_password_on_focus_;

  // Number of verdict stored for this profile for protected password entry
  // pings.
  base::Optional<size_t> stored_verdict_count_password_entry_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  // Content settings maps associated with this instance.
  scoped_refptr<HostContentSettingsMap> content_settings_;

  base::WeakPtrFactory<VerdictCacheManager> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_VERDICT_CACHE_MANAGER_H_
