// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_

#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"

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
};

// Returns whether the threat type is relevant for hash-prefix real-time
// lookups.
bool IsThreatTypeRelevant(const V5::ThreatType& threat_type);

// Returns the 4-byte prefix of the requested 32-byte full hash.
std::string GetHashPrefix(const std::string& full_hash);

// Specifies whether hash-prefix real-time lookups are possible for the
// browser session. This function should never take in parameters.
bool IsHashRealTimeLookupEligibleInSession();

// Based on the user's settings and session, determines which hash-prefix
// real-time lookup should be used, if any.
HashRealTimeSelection DetermineHashRealTimeSelection(bool is_off_the_record,
                                                     PrefService* prefs);

// A helper for consumers that want to recompute DetermineHashRealTimeSelection
// when there are pref changes. This returns all prefs that modify the outcome
// of that method.
std::vector<const char*> GetHashRealTimeSelectionConfiguringPrefs();

}  // namespace safe_browsing::hash_realtime_utils

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
