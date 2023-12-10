// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// Serves as a cache for CSD-Phishing's local verdicts. Both CSD-Phishing and
// PhishGuard are expected to be clients of this cache.
class ClientSideDetectionFeatureCache
    : public content::WebContentsUserData<ClientSideDetectionFeatureCache> {
 public:
  explicit ClientSideDetectionFeatureCache(content::WebContents* web_contents);
  ~ClientSideDetectionFeatureCache() override;
  ClientSideDetectionFeatureCache(const ClientSideDetectionFeatureCache&) =
      delete;
  ClientSideDetectionFeatureCache& operator=(
      const ClientSideDetectionFeatureCache&) = delete;

  // When inserting a ClientPhishingRequest, we will override an old message
  // object, if it exists, because new models can potentially give different
  // output images.
  void Insert(const GURL& url, std::unique_ptr<ClientPhishingRequest> verdict);

  // When fetching the proto message, we will also remove it from the map.
  ClientPhishingRequest* GetFeatureMapForURL(const GURL& url);

  void AddClearCacheSubscription(
      base::WeakPtr<ClientSideDetectionService> csd_service);

  size_t GetMaxMapCapacity();
  long GetTotalFeatureMapEntriesSize();

 private:
  friend class content::WebContentsUserData<ClientSideDetectionFeatureCache>;

  void Clear();

  base::flat_map<GURL, std::unique_ptr<ClientPhishingRequest>> verdict_map_;
  base::queue<GURL> gurl_queue_;
  static constexpr size_t kMaxMapCapacity = 10;
  base::CallbackListSubscription clear_cache_subscription_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
