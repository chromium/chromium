// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_

#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

class PrefService;

// These are utils for hash-prefix real-time lookups.
namespace safe_browsing::hash_realtime_utils {
constexpr size_t kHashPrefixLength = 4;  // bytes
constexpr size_t kFullHashLength = 32;   // bytes

// Specifies which hash-prefix real-time lookup should be used.
enum class HashRealTimeSelection {
  // There should not be any lookup.
  kNone = 0,
  // The lookup performed should use the native HashRealTimeService. This is
  // relevant to Desktop and iOS.
  kHashRealTimeService = 1,
  // The lookup performed should use the database manager. This is relevant to
  // Android (Chrome and WebView).
  kDatabaseManager = 2,
};

// Used only for tests. This is useful so that more than just
// GOOGLE_CHROME_BRANDING bots are capable of testing code that only runs when
// |IsHashRealTimeLookupEligibleInSession| returns true. To allow pretending
// that there is Google Chrome branding on unbranded builds, create an object
// of this type and keep it in scope for as long as the override should exist.
// The constructor will set the override, and the destructor will clear it. If
// necessary, the override can be cleared by calling |StopApplyingBranding|.
class GoogleChromeBrandingPretenderForTesting {
 public:
  GoogleChromeBrandingPretenderForTesting();
  GoogleChromeBrandingPretenderForTesting(
      const GoogleChromeBrandingPretenderForTesting&) = delete;
  GoogleChromeBrandingPretenderForTesting& operator=(
      const GoogleChromeBrandingPretenderForTesting&) = delete;
  ~GoogleChromeBrandingPretenderForTesting();

  // The normal way to stop applying branding is for this object to go out of
  // scope. However, if necessary it can also be done manually by calling this
  // method.
  void StopApplyingBranding();
};

// Returns whether the |url| is eligible for hash-prefix real-time checks.
// It's never eligible if the |request_destination| is not mainframe.
bool CanCheckUrl(const GURL& url,
                 network::mojom::RequestDestination request_destination);

// Returns whether the threat type is relevant for hash-prefix real-time
// lookups.
bool IsThreatTypeRelevant(const V5::ThreatType& threat_type);

// Returns the 4-byte prefix of the requested 32-byte full hash.
std::string GetHashPrefix(const std::string& full_hash);

// Specifies whether hash-prefix real-time lookups are possible for the
// browser session. This function should never take in parameters.
bool IsHashRealTimeLookupEligibleInSession();

// Based on the user's settings and session, determines which hash-prefix
// real-time lookup should be used, if any. If |log_usage_histograms| is true,
// this will log metrics related to whether hash real-time lookups were
// available or why not.
HashRealTimeSelection DetermineHashRealTimeSelection(
    bool is_off_the_record,
    PrefService* prefs,
    bool log_usage_histograms = false);

// A helper for consumers that want to recompute DetermineHashRealTimeSelection
// when there are pref changes. This returns all prefs that modify the outcome
// of that method.
std::vector<const char*> GetHashRealTimeSelectionConfiguringPrefs();

}  // namespace safe_browsing::hash_realtime_utils

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
