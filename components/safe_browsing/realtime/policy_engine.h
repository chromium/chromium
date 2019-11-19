// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
#define COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_

#include "build/build_config.h"
#include "content/public/common/resource_type.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {

#if defined(OS_ANDROID)
// A parameter controlled by finch experiment.
// On Android, performs real time URL lookup only if |kRealTimeUrlLookupEnabled|
// is enabled, and system memory is larger than threshold.
const char kRealTimeUrlLookupMemoryThresholdMb[] =
    "SafeBrowsingRealTimeUrlLookupMemoryThresholdMb";
#endif

// This class implements the logic to decide whether the real time lookup
// feature is enabled for a given user/profile.
class RealTimePolicyEngine {
 public:
  RealTimePolicyEngine() = delete;
  ~RealTimePolicyEngine() = delete;

  // Return true if full URL lookups are enabled for |resource_type|.
  static bool CanPerformFullURLLookupForResourceType(
      content::ResourceType resource_type);

  // Return true if the feature to enable full URL lookups is enabled and the
  // allowlist fetch is enabled for the profile represented by
  // |browser_context|.
  static bool CanPerformFullURLLookup(content::BrowserContext* browser_context);

  friend class SafeBrowsingService;

 private:
  // Is the feature to perform real-time URL lookup enabled?
  static bool IsUrlLookupEnabled();

  // Is user opted-in to the feature?
  static bool IsUserOptedIn(content::BrowserContext* browser_context);

  // Is the feature enabled due to enterprise policy?
  static bool IsEnabledByPolicy(content::BrowserContext* browser_context);

  friend class RealTimePolicyEngineTest;
};  // class RealTimePolicyEngine

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_REALTIME_POLICY_ENGINE_H_
