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

// Represents the source of the caller.
enum class ApiCallerSource {
  // The API usage is from document.browsingTopics().
  kJavaScript,

  // The API usage is from fetch(<url>, {browsingTopics: true}).
  kFetch,
};

// Represents the different reasons why the topics API access is denied. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class ApiAccessFailureReason {
  // The requesting context doesn't allow the API (e.g. permissions policy).
  kInvalidRequestingContext = 0,

  // The topics state hasn't finished loading.
  kStateNotReady = 1,

  // Access is disallowed by user settings.
  kAccessDisallowedBySettings = 2,

  kMaxValue = kAccessDisallowedBySettings,
};

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
