// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"

#include "content/public/browser/web_contents_user_data.h"

using content::WebContents;

namespace safe_browsing {

ClientSideDetectionFeatureCache::ClientSideDetectionFeatureCache(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ClientSideDetectionFeatureCache>(
          *web_contents) {}

ClientSideDetectionFeatureCache::~ClientSideDetectionFeatureCache() = default;

void ClientSideDetectionFeatureCache::Insert(
    const GURL& url,
    std::unique_ptr<ClientPhishingRequest> verdict) {
  verdict_map_[url] = std::move(verdict);
  gurl_queue_.push(url);

  while (verdict_map_.size() > kMaxMapCapacity) {
    GURL popped_url = gurl_queue_.front();
    verdict_map_.erase(popped_url);
    gurl_queue_.pop();
  }
}

ClientPhishingRequest* ClientSideDetectionFeatureCache::GetFeatureMapForURL(
    const GURL& url) {
  auto it = verdict_map_.find(url);
  if (it == verdict_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

size_t ClientSideDetectionFeatureCache::GetMaxMapCapacity() {
  return kMaxMapCapacity;
}

long ClientSideDetectionFeatureCache::GetTotalFeatureMapEntriesSize() {
  long total_verdicts_size = 0;
  for (auto& it : verdict_map_) {
    total_verdicts_size += it.second->ByteSizeLong();
  }

  return total_verdicts_size;
}

void ClientSideDetectionFeatureCache::AddClearCacheSubscription(
    base::WeakPtr<ClientSideDetectionService> csd_service) {
  clear_cache_subscription_ =
      csd_service->RegisterCallbackForModelUpdates(base::BindRepeating(
          &ClientSideDetectionFeatureCache::Clear, base::Unretained(this)));
}

void ClientSideDetectionFeatureCache::Clear() {
  verdict_map_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientSideDetectionFeatureCache);

}  // namespace safe_browsing
