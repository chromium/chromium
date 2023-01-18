// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "base/check.h"

namespace safe_browsing::hash_realtime_utils {
bool IsThreatTypeRelevant(const V5::ThreatType& threat_type) {
  switch (threat_type) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::UNWANTED_SOFTWARE:
    case V5::ThreatType::SUSPICIOUS:
    case V5::ThreatType::TRICK_TO_BILL:
      return true;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return false;
  }
}
std::string GetHashPrefix(const std::string& full_hash) {
  DCHECK(full_hash.length() == kFullHashLength);
  return full_hash.substr(0, kHashPrefixLength);
}

}  // namespace safe_browsing::hash_realtime_utils
