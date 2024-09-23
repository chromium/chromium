// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"

#include "base/check_op.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/web_contents_user_data.h"

using content::WebContents;

namespace safe_browsing {

using DebuggingMetadata = LoginReputationClientRequest::DebuggingMetadata;

ClientSideDetectionFeatureCache::ClientSideDetectionFeatureCache(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ClientSideDetectionFeatureCache>(
          *web_contents) {}

ClientSideDetectionFeatureCache::~ClientSideDetectionFeatureCache() = default;

void ClientSideDetectionFeatureCache::InsertVerdict(
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

ClientPhishingRequest* ClientSideDetectionFeatureCache::GetVerdictForURL(
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

long ClientSideDetectionFeatureCache::GetTotalVerdictEntriesSize() {
  long total_verdicts_size = 0;
  for (auto& it : verdict_map_) {
    total_verdicts_size += it.second->ByteSizeLong();
  }

  return total_verdicts_size;
}

LoginReputationClientRequest::DebuggingMetadata*
ClientSideDetectionFeatureCache::GetOrCreateDebuggingMetadataForURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DebuggingMetadata* debugging_metadata = GetDebuggingMetadataForURL(url);

  if (!debugging_metadata) {
    std::unique_ptr<DebuggingMetadata> new_debugging_metadata(
        new DebuggingMetadata);

    debug_metadata_map_[url] = std::move(new_debugging_metadata);
    debugging_metadata_deque_.push_back(url);

    while (debug_metadata_map_.size() > kMaxMapCapacity) {
      GURL popped_url = debugging_metadata_deque_.front();
      RemoveDebuggingMetadataForURL(popped_url);
    }

    debugging_metadata = GetDebuggingMetadataForURL(url);
  }

  return debugging_metadata;
}

bool ClientSideDetectionFeatureCache::WasVibrationClassificationTriggered(
    const GURL& vibration_requested_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time cutoff = base::Time::Now() - kVibrationReportsInterval;
  for (std::pair<GURL, base::Time> url :
       vibration_api_classification_times_map_) {
    if (url.second < cutoff) {
      vibration_api_classification_times_map_.erase(url.first);
    }
  }

  if (vibration_api_classification_times_map_.contains(
          vibration_requested_url)) {
    return true;
  } else {
    vibration_api_classification_times_map_[vibration_requested_url] =
        base::Time::Now();
    return false;
  }
}

void ClientSideDetectionFeatureCache::RemoveDebuggingMetadataForURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The remove function is called when PW-Reuse is called, and we want to
  // ensure that the most recent debugging metadata is the URL that the
  // PW-Reuse happened on. We do not need to check whether the map contains the
  // url because the erase function handles that already.
  debug_metadata_map_.erase(url);
  for (std::deque<GURL>::iterator it = debugging_metadata_deque_.begin();
       it != debugging_metadata_deque_.end();) {
    if (*it == url) {
      it = debugging_metadata_deque_.erase(it);
      break;
    }
    ++it;
  }
}

DebuggingMetadata* ClientSideDetectionFeatureCache::GetDebuggingMetadataForURL(
    const GURL& url) {
  auto it = debug_metadata_map_.find(url);
  if (it == debug_metadata_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

long ClientSideDetectionFeatureCache::
    GetTotalDebuggingMetadataMapEntriesSize() {
  long total_debugging_metadata_size = 0;
  for (auto& it : debug_metadata_map_) {
    total_debugging_metadata_size += it.second->ByteSizeLong();
  }

  return total_debugging_metadata_size;
}

void ClientSideDetectionFeatureCache::AddClearCacheSubscription(
    base::WeakPtr<ClientSideDetectionService> csd_service) {
  clear_cache_subscription_ =
      csd_service->RegisterCallbackForModelUpdates(base::BindRepeating(
          &ClientSideDetectionFeatureCache::Clear, base::Unretained(this)));
}

void ClientSideDetectionFeatureCache::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  verdict_map_.clear();
  base::queue<GURL> empty_gurl;
  gurl_queue_.swap(empty_gurl);

  debug_metadata_map_.clear();
  debugging_metadata_deque_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientSideDetectionFeatureCache);

}  // namespace safe_browsing
