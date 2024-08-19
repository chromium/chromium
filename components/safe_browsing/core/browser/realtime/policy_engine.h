// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_POLICY_ENGINE_H_

#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"

class PrefService;

namespace variations {
class VariationsService;
}

namespace safe_browsing {

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
// TODO(crbug.com/40673388): To make this class build in IOS, remove
// browser_context dependency in this class, and replace it with pref_service
// and simple_factory_key.
class RealTimePolicyEngine {
 public:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

  // A callback via which the client of this class indicates whether they
  // are configured to support token fetches. Used as part of
  // CanPerformFullURLLookupWithToken().
  using ClientConfiguredForTokenFetchesCallback =
      base::OnceCallback<bool(bool user_has_enabled_enhanced_protection)>;

  // Return true if the profile is not Incognito and real-time fetches are
  // available in the user's country, and the user has opted in to ESB or MBB.
  static bool CanPerformFullURLLookup(
      PrefService* pref_service,
      bool is_off_the_record,
      variations::VariationsService* variations_service);

  // Return true if the OAuth token should be associated with the URL lookup
  // pings.
  static bool CanPerformFullURLLookupWithToken(
      PrefService* pref_service,
      bool is_off_the_record,
      ClientConfiguredForTokenFetchesCallback client_callback,
      variations::VariationsService* variations_service);

  static bool CanPerformEnterpriseFullURLLookup(const PrefService* pref_service,
                                                bool has_valid_dm_token,
                                                bool is_off_the_record,
                                                bool is_guest_profile);

  friend class SafeBrowsingService;

 private:
  static bool IsInExcludedCountry(const std::string& country_code);

  // Whether the user has opted-in to MBB.
  static bool IsUserMbbOptedIn(PrefService* pref_service);

  // Whether the user has opted-in to Enhanced Protection.
  static bool IsUserEpOptedIn(PrefService* pref_service);

  friend class RealTimePolicyEngineTest;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REALTIME_POLICY_ENGINE_H_
