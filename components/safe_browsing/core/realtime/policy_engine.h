// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_

#include <string>

#include "build/build_config.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace signin {
class IdentityManager;
}

namespace variations {
class VariationsService;
}

namespace safe_browsing {

enum class ResourceType;

#if defined(OS_ANDROID)
// A parameter controlled by finch experiment.
// On Android, performs real time URL lookup only if |kRealTimeUrlLookupEnabled|
// is enabled, and system memory is larger than threshold.
const char kRealTimeUrlLookupMemoryThresholdMb[] =
    "SafeBrowsingRealTimeUrlLookupMemoryThresholdMb";
#endif

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
// TODO(crbug.com/1050859): To make this class build in IOS, remove
// browser_context dependency in this class, and replace it with pref_service
// and simple_factory_key.
class RealTimePolicyEngine {
 public:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

  // Return true if full URL lookups are enabled for |resource_type|. If
  // |can_rt_check_subresource_url| is set to false, return true only if
  // |resource_type| is |kMainFrame|.
  static bool CanPerformFullURLLookupForResourceType(
      ResourceType resource_type,
      bool can_rt_check_subresource_url);

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled for the profile represented by
  // |pref_service|.
  static bool CanPerformFullURLLookup(
      PrefService* pref_service,
      bool is_off_the_record,
      variations::VariationsService* variations_service);

  // Return true if the OAuth token should be associated with the URL lookup
  // pings.
  static bool CanPerformFullURLLookupWithToken(
      PrefService* pref_service,
      bool is_off_the_record,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      variations::VariationsService* variations_service);

  static bool CanPerformEnterpriseFullURLLookup(const PrefService* pref_service,
                                                bool has_valid_dm_token,
                                                bool is_off_the_record);

  friend class SafeBrowsingService;

 private:
  static bool IsInExcludedCountry(const std::string& country_code);

  // Is the feature to perform real-time URL lookup enabled?
  static bool IsUrlLookupEnabled();

  // Is the feature to perform real-time URL lookup enabled for enhanced
  // protection users?
  static bool IsUrlLookupEnabledForEp();

  // Is the feature to include OAuth tokens with real-time URL lookup enabled
  // for Enhanced Protection users?
  static bool IsUrlLookupEnabledForEpWithToken();

  // Whether the user has opted-in to MBB.
  static bool IsUserMbbOptedIn(PrefService* pref_service);

  // Whether the user has opted-in to Enhanced Protection.
  static bool IsUserEpOptedIn(PrefService* pref_service);

  // Whether the primary account is signed in. Sync is not required.
  static bool IsPrimaryAccountSignedIn(
      signin::IdentityManager* identity_manager);

  friend class RealTimePolicyEngineTest;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_REALTIME_POLICY_ENGINE_H_
