// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_

#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_feature_cache_base.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// Serves as a cache for CSD-Phishing's local verdicts. Both CSD-Phishing and
// PhishGuard are expected to be clients of this cache.
class ClientSideDetectionFeatureCache
    : public content::WebContentsUserData<ClientSideDetectionFeatureCache>,
      public ClientSideDetectionFeatureCacheBase {
 public:
  explicit ClientSideDetectionFeatureCache(content::WebContents* web_contents);
  ~ClientSideDetectionFeatureCache() override;
  ClientSideDetectionFeatureCache(const ClientSideDetectionFeatureCache&) =
      delete;
  ClientSideDetectionFeatureCache& operator=(
      const ClientSideDetectionFeatureCache&) = delete;

 private:
  friend class content::WebContentsUserData<ClientSideDetectionFeatureCache>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
