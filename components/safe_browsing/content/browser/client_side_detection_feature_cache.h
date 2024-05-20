// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_

#include <deque>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "base/sequence_checker.h"
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
  void InsertVerdict(const GURL& url,
                     std::unique_ptr<ClientPhishingRequest> verdict);
  ClientPhishingRequest* GetVerdictForURL(const GURL& url);

  void AddClearCacheSubscription(
      base::WeakPtr<ClientSideDetectionService> csd_service);

  size_t GetMaxMapCapacity();
  long GetTotalVerdictEntriesSize();

  // Checks whether the vibration API was triggered for a given URL within the
  // last |kVibrationReportsInterval|. If not found, insert into the times map.
  bool WasVibrationClassificationTriggered(const GURL& url);

  // The following functions are related to caching debugging metadata for
  // PhishGuard pings.
  LoginReputationClientRequest::DebuggingMetadata*
  GetOrCreateDebuggingMetadataForURL(const GURL& url);
  void RemoveDebuggingMetadataForURL(const GURL& url);

  long GetTotalDebuggingMetadataMapEntriesSize();

  LoginReputationClientRequest::DebuggingMetadata* GetDebuggingMetadataForURL(
      const GURL& url);

 private:
  friend class content::WebContentsUserData<ClientSideDetectionFeatureCache>;
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      CheckDebuggingMetadataCacheAfterClearingCacheAfterNavigation);

  void Clear();

  base::flat_map<GURL, std::unique_ptr<ClientPhishingRequest>> verdict_map_;
  base::flat_map<
      GURL,
      std::unique_ptr<LoginReputationClientRequest::DebuggingMetadata>>
      debug_metadata_map_;
  base::flat_map<GURL, base::Time> vibration_api_classification_times_map_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::queue<GURL> gurl_queue_;
  std::deque<GURL> debugging_metadata_deque_
      GUARDED_BY_CONTEXT(sequence_checker_);
  static constexpr size_t kMaxMapCapacity = 10;
  static constexpr base::TimeDelta kVibrationReportsInterval = base::Days(1);
  base::CallbackListSubscription clear_cache_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_FEATURE_CACHE_H_
