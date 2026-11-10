// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_FEATURE_CACHE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_FEATURE_CACHE_BASE_H_

#include <deque>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

class ClientSideDetectionServiceBase;

class ClientSideDetectionFeatureCacheBase {
 public:
  ClientSideDetectionFeatureCacheBase();
  virtual ~ClientSideDetectionFeatureCacheBase();

  // When inserting a ClientPhishingRequest, we will override an old message
  // object, if it exists, because new models can potentially give different
  // output images.
  void InsertVerdict(const GURL& url,
                     std::unique_ptr<ClientPhishingRequest> verdict);
  // Retrieves the cached phishing verdict for a given URL, or nullptr if not
  // found.
  ClientPhishingRequest* GetVerdictForURL(const GURL& url);

  // Subscribes to model update events from the CSD service to automatically
  // clear the cache when a new model is deployed.
  void AddClearCacheSubscription(
      base::WeakPtr<ClientSideDetectionServiceBase> csd_service);

  // Returns the maximum number of entries allowed in each of the internal maps.
  size_t GetMaxMapCapacity();

  // Returns the sum of the byte sizes of all cached verdict objects.
  long GetTotalVerdictEntriesSize();

  // Caching debugging metadata for PhishGuard pings. Retrieves the existing
  // metadata or inserts a new one if missing.
  LoginReputationClientRequest::DebuggingMetadata*
  GetOrCreateDebuggingMetadataForURL(const GURL& url);

  // Removes the cached debugging metadata for a given URL if it exists.
  void RemoveDebuggingMetadataForURL(const GURL& url);

  // Retrieves the cached debugging metadata for a given URL, or nullptr if not
  // found.
  LoginReputationClientRequest::DebuggingMetadata* GetDebuggingMetadataForURL(
      const GURL& url);

  // Returns the sum of the byte sizes of all cached debugging metadata objects.
  long GetTotalDebuggingMetadataMapEntriesSize();

  // Clears all cached verdicts and debugging metadata.
  void ClearForTesting();

 private:
  void Clear();

  base::flat_map<GURL, std::unique_ptr<ClientPhishingRequest>> verdict_map_;
  base::flat_map<
      GURL,
      std::unique_ptr<LoginReputationClientRequest::DebuggingMetadata>>
      debug_metadata_map_;
  base::queue<GURL> gurl_queue_;
  std::deque<GURL> debugging_metadata_deque_
      GUARDED_BY_CONTEXT(sequence_checker_);
  static constexpr size_t kMaxMapCapacity = 10;
  base::CallbackListSubscription clear_cache_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_FEATURE_CACHE_BASE_H_
