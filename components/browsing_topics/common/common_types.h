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

// Explicitly typed config version.
enum ConfigVersion {
  kInitial = 1,
  kUsePrioritizedTopicsList = 2,

  kMaxValue = kUsePrioritizedTopicsList,
};

// Returns the current configuration version.
COMPONENT_EXPORT(BROWSING_TOPICS_COMMON) ConfigVersion CurrentConfigVersion();

// Represents the source of the caller.
enum class ApiCallerSource {
  // The API usage is from document.browsingTopics().
  kJavaScript,

  // The API usage is from fetch-like APIs. That is,
  // fetch(<url>, {browsingTopics: true}), or XMLHttpRequest.send() with the
  // `deprecatedBrowsingTopics` property set to true.
  kFetch,

  // The API usage is from <iframe src=[url] browsingtopics>.
  kIframeAttribute,
};

// Represents the different reasons why the topics API access is denied. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class ApiAccessResult {
  // The requesting context doesn't allow the API (e.g. permissions policy).
  kInvalidRequestingContext = 0,

  // The topics state hasn't finished loading.
  kStateNotReady = 1,

  // Access is disallowed by user settings.
  kAccessDisallowedBySettings = 2,

  // Call was a success and not a failure.
  kSuccess = 3,

  kMaxValue = kSuccess,
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
