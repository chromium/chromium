// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/test_util.h"

namespace browsing_topics {

TesterBrowsingTopicsCalculator::TesterBrowsingTopicsCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    optimization_guide::PageContentAnnotationsService* annotations_service,
    CalculateCompletedCallback callback,
    base::queue<uint64_t> rand_uint64_queue)
    : BrowsingTopicsCalculator(privacy_sandbox_settings,
                               history_service,
                               site_data_manager,
                               annotations_service,
                               std::move(callback)),
      rand_uint64_queue_(std::move(rand_uint64_queue)) {}

TesterBrowsingTopicsCalculator::~TesterBrowsingTopicsCalculator() = default;

uint64_t TesterBrowsingTopicsCalculator::GenerateRandUint64() {
  DCHECK(!rand_uint64_queue_.empty());

  uint64_t next_rand_uint64 = rand_uint64_queue_.front();
  rand_uint64_queue_.pop();

  return next_rand_uint64;
}

}  // namespace browsing_topics
