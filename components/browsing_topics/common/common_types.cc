// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/common/common_types.h"

#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

ConfigVersion CurrentConfigVersion() {
  if (!blink::features::kBrowsingTopicsPrioritizedTopicsList.Get().empty()) {
    return ConfigVersion::kUsePrioritizedTopicsList;
  } else {
    return ConfigVersion::kInitial;
  }
}

ApiUsageContextQueryResult::ApiUsageContextQueryResult() = default;

ApiUsageContextQueryResult::ApiUsageContextQueryResult(
    std::vector<ApiUsageContext> api_usage_contexts)
    : success(true), api_usage_contexts(std::move(api_usage_contexts)) {}

ApiUsageContextQueryResult::ApiUsageContextQueryResult(
    ApiUsageContextQueryResult&& other) = default;

ApiUsageContextQueryResult& ApiUsageContextQueryResult::operator=(
    ApiUsageContextQueryResult&& other) = default;

ApiUsageContextQueryResult::~ApiUsageContextQueryResult() = default;

}  // namespace browsing_topics
