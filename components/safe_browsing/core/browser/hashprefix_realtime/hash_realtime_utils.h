// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_

#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"

// These are utils for hash-prefix real-time lookups.
namespace safe_browsing::hash_realtime_utils {
constexpr size_t kHashPrefixLength = 4;  // bytes
constexpr size_t kFullHashLength = 32;   // bytes

// Returns whether the threat type is relevant for hash-prefix real-time
// lookups.
bool IsThreatTypeRelevant(const V5::ThreatType& threat_type);

// Returns the 4-byte prefix of the requested 32-byte full hash.
std::string GetHashPrefix(const std::string& full_hash);

}  // namespace safe_browsing::hash_realtime_utils

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_HASH_REALTIME_UTILS_H_
