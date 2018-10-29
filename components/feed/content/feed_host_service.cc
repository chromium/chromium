// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_host_service.h"

#include <utility>

namespace feed {

FeedHostService::FeedHostService(
    std::unique_ptr<FeedLoggingMetrics> logging_metrics,
    std::unique_ptr<FeedImageManager> image_manager,
    std::unique_ptr<FeedNetworkingHost> networking_host,
    std::unique_ptr<FeedSchedulerHost> scheduler_host,
    std::unique_ptr<FeedContentDatabase> content_database,
    std::unique_ptr<FeedJournalDatabase> journal_database,
    std::unique_ptr<FeedOfflineHost> offline_host)
    : logging_metrics_(std::move(logging_metrics)),
      image_manager_(std::move(image_manager)),
      networking_host_(std::move(networking_host)),
      scheduler_host_(std::move(scheduler_host)),
      content_database_(std::move(content_database)),
      journal_database_(std::move(journal_database)),
      offline_host_(std::move(offline_host)) {}

FeedHostService::~FeedHostService() = default;

FeedImageManager* FeedHostService::GetImageManager() {
  return image_manager_.get();
}

FeedNetworkingHost* FeedHostService::GetNetworkingHost() {
  return networking_host_.get();
}

FeedSchedulerHost* FeedHostService::GetSchedulerHost() {
  return scheduler_host_.get();
}

FeedContentDatabase* FeedHostService::GetContentDatabase() {
  return content_database_.get();
}

FeedJournalDatabase* FeedHostService::GetJournalDatabase() {
  return journal_database_.get();
}

FeedOfflineHost* FeedHostService::GetOfflineHost() {
  return offline_host_.get();
}

FeedLoggingMetrics* FeedHostService::GetLoggingMetrics() {
  return logging_metrics_.get();
}

}  // namespace feed
