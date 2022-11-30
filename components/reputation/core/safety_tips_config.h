// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPUTATION_CORE_SAFETY_TIPS_CONFIG_H_
#define COMPONENTS_REPUTATION_CORE_SAFETY_TIPS_CONFIG_H_

#include <memory>
#include <string>

#include "components/reputation/core/safety_tips.pb.h"
#include "components/security_state/core/security_state.h"

class GURL;

namespace reputation {

// Sets the global configuration for Safety Tips retrieved from the component
// updater. The configuration proto contains the list of URLs that can trigger
// a safety tip.
void SetSafetyTipsRemoteConfigProto(std::unique_ptr<SafetyTipsConfig> proto);

// Gets the global configuration for Safety Tips as retrieved from the component
// updater. The configuration proto contains the list of URLs that can trigger
// a safety tip.
const SafetyTipsConfig* GetSafetyTipsRemoteConfigProto();

// Checks permutations of |visited_url| against the component updater allowlist
// and returns whether the URL is explicitly allowed to spoof |canonical_url|.
//
// Cases when canonical_url is unknown (as in kFailedSpoofChecks) are treated as
// if they're trying to spoof themselves, so set canonical_url = visited_url.
bool IsUrlAllowlistedBySafetyTipsComponent(const SafetyTipsConfig* proto,
                                           const GURL& visited_url,
                                           const GURL& canonical_url);

// Checks |hostname| against the component updater target allowlist and returns
// whether it is explicitly allowed.
bool IsTargetHostAllowlistedBySafetyTipsComponent(const SafetyTipsConfig* proto,
                                                  const std::string& hostname);

// Checks SafeBrowsing-style permutations of |url| against the component updater
// blocklist and returns the match type. kNone means the URL is not blocked.
// This method assumes that the flagged pages in the safety tip config proto are
// in sorted order.
security_state::SafetyTipStatus GetSafetyTipUrlBlockType(const GURL& url);

// Returns whether |word| is included in the component updater common word list
bool IsCommonWordInConfigProto(const SafetyTipsConfig* proto,
                               const std::string& word);

}  // namespace reputation

#endif  // COMPONENTS_REPUTATION_CORE_SAFETY_TIPS_CONFIG_H_
