// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_

#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_feature_cache_base.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

namespace safe_browsing {

// Serves as a cache for CSD-Phishing's local verdicts. Both CSD-Phishing and
// PhishGuard are expected to be clients of this cache.
class ClientSideDetectionFeatureCache
    : public web::WebStateUserData<ClientSideDetectionFeatureCache>,
      public ClientSideDetectionFeatureCacheBase {
 public:
  ~ClientSideDetectionFeatureCache() override;
  ClientSideDetectionFeatureCache(const ClientSideDetectionFeatureCache&) =
      delete;
  ClientSideDetectionFeatureCache& operator=(
      const ClientSideDetectionFeatureCache&) = delete;

 private:
  friend class web::WebStateUserData<ClientSideDetectionFeatureCache>;

  explicit ClientSideDetectionFeatureCache(web::WebState* web_state);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
