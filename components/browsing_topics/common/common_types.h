// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_
#define COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_

#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"

namespace browsing_topics {

using HashedHost = base::StrongAlias<class HashedHostTag, int64_t>;
using HashedDomain = base::StrongAlias<class HashedHostTag, int64_t>;
using Topic = base::StrongAlias<class TopicTag, int>;

struct COMPONENT_EXPORT(BROWSING_TOPICS_COMMON) ApiUsageContext {
  HashedDomain hashed_context_domain;
  HashedHost hashed_main_frame_host;
  base::Time time;
};

struct COMPONENT_EXPORT(BROWSING_TOPICS_COMMON) ApiUsageContextQueryResult {
  ApiUsageContextQueryResult();
  explicit ApiUsageContextQueryResult(
      std::vector<ApiUsageContext> api_usage_contexts);

  ApiUsageContextQueryResult(const ApiUsageContextQueryResult&) = delete;
  ApiUsageContextQueryResult& operator=(const ApiUsageContextQueryResult&) =
      delete;

  ApiUsageContextQueryResult(ApiUsageContextQueryResult&&);
  ApiUsageContextQueryResult& operator=(ApiUsageContextQueryResult&&);

  ~ApiUsageContextQueryResult();

  bool success = false;

  std::vector<ApiUsageContext> api_usage_contexts;
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_
