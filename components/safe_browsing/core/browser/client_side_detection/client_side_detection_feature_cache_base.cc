// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_feature_cache_base.h"

#include <memory>

#include "base/check_op.h"
#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_service_base.h"

namespace safe_browsing {

using DebuggingMetadata = LoginReputationClientRequest::DebuggingMetadata;

ClientSideDetectionFeatureCacheBase::ClientSideDetectionFeatureCacheBase() =
    default;
ClientSideDetectionFeatureCacheBase::~ClientSideDetectionFeatureCacheBase() =
    default;

void ClientSideDetectionFeatureCacheBase::InsertVerdict(
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

ClientPhishingRequest* ClientSideDetectionFeatureCacheBase::GetVerdictForURL(
    const GURL& url) {
  auto it = verdict_map_.find(url);
  if (it == verdict_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

size_t ClientSideDetectionFeatureCacheBase::GetMaxMapCapacity() {
  return kMaxMapCapacity;
}

long ClientSideDetectionFeatureCacheBase::GetTotalVerdictEntriesSize() {
  long total_verdicts_size = 0;
  for (auto& it : verdict_map_) {
    total_verdicts_size += it.second->ByteSizeLong();
  }

  return total_verdicts_size;
}

LoginReputationClientRequest::DebuggingMetadata*
ClientSideDetectionFeatureCacheBase::GetOrCreateDebuggingMetadataForURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DebuggingMetadata* debugging_metadata = GetDebuggingMetadataForURL(url);

  if (!debugging_metadata) {
    std::unique_ptr<DebuggingMetadata> new_debugging_metadata =
        std::make_unique<DebuggingMetadata>();

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

void ClientSideDetectionFeatureCacheBase::RemoveDebuggingMetadataForURL(
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

DebuggingMetadata*
ClientSideDetectionFeatureCacheBase::GetDebuggingMetadataForURL(
    const GURL& url) {
  auto it = debug_metadata_map_.find(url);
  if (it == debug_metadata_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

long ClientSideDetectionFeatureCacheBase::
    GetTotalDebuggingMetadataMapEntriesSize() {
  long total_debugging_metadata_size = 0;
  for (auto& it : debug_metadata_map_) {
    total_debugging_metadata_size += it.second->ByteSizeLong();
  }

  return total_debugging_metadata_size;
}

void ClientSideDetectionFeatureCacheBase::AddClearCacheSubscription(
    base::WeakPtr<ClientSideDetectionServiceBase> csd_service) {
  clear_cache_subscription_ =
      csd_service->RegisterCallbackForModelUpdates(base::BindRepeating(
          &ClientSideDetectionFeatureCacheBase::Clear, base::Unretained(this)));
}

void ClientSideDetectionFeatureCacheBase::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  verdict_map_.clear();
  base::queue<GURL> empty_gurl;
  gurl_queue_.swap(empty_gurl);

  debug_metadata_map_.clear();
  debugging_metadata_deque_.clear();
}

void ClientSideDetectionFeatureCacheBase::ClearForTesting() {
  Clear();
}

}  // namespace safe_browsing
