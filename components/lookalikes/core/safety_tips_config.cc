// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/safety_tips_config.h"

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using safe_browsing::V4ProtocolManagerUtil;

namespace lookalikes {

namespace {

class SafetyTipsConfigSingleton {
 public:
  void SetProto(std::unique_ptr<reputation::SafetyTipsConfig> proto) {
    proto_ = std::move(proto);
  }

  reputation::SafetyTipsConfig* GetProto() const { return proto_.get(); }

  static SafetyTipsConfigSingleton& GetInstance() {
    static base::NoDestructor<SafetyTipsConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<reputation::SafetyTipsConfig> proto_;
};

// Given a URL, generates all possible variant URLs to check the blocklist for.
// This is conceptually almost identical to safe_browsing::UrlToFullHashes, but
// without the hashing step.
//
// Note: Blocking "a.b/c/" does NOT block http://a.b/c without the trailing /.
void UrlToSafetyTipPatterns(const GURL& url,
                            std::vector<std::string>* patterns) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    V4ProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
                                                     &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      DCHECK(path.length() == 0 || path[0] == '/');
      patterns->push_back(host + path);
    }
  }
}

// Return whether |canonical_url| is a member of the designated cohort.
bool IsUrlAllowedByCohort(const reputation::SafetyTipsConfig* proto,
                          const GURL& canonical_url,
                          unsigned cohort_index) {
  DCHECK(proto);
  DCHECK(canonical_url.is_valid());

  // Ensure that the cohort index is valid before using it. If it isn't valid,
  // we just pretend the cohort didn't include the canonical URL.
  if (cohort_index >= static_cast<unsigned>(proto->cohort_size())) {
    return false;
  }

  const auto& cohort = proto->cohort(cohort_index);

  // For each possible URL pattern, see if any of the indicated allowed_index or
  // canonical_index entries correspond to a matching pattern since both sets of
  // indices are considered valid spoof targets.
  std::vector<std::string> patterns;
  UrlToSafetyTipPatterns(canonical_url, &patterns);
  for (const auto& search_pattern : patterns) {
    for (const unsigned allowed_index : cohort.allowed_index()) {
      // Skip over invalid indices.
      if (allowed_index >=
          static_cast<unsigned>(proto->allowed_pattern_size())) {
        continue;
      }
      const auto& pattern = proto->allowed_pattern(allowed_index).pattern();
      if (pattern == search_pattern) {
        return true;
      }
    }
    for (const unsigned canonical_index : cohort.canonical_index()) {
      // Skip over invalid indices.
      if (canonical_index >=
          static_cast<unsigned>(proto->canonical_pattern_size())) {
        continue;
      }
      const auto& pattern = proto->canonical_pattern(canonical_index).pattern();
      if (pattern == search_pattern) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

void SetSafetyTipsRemoteConfigProto(
    std::unique_ptr<reputation::SafetyTipsConfig> proto) {
  SafetyTipsConfigSingleton::GetInstance().SetProto(std::move(proto));
}

const reputation::SafetyTipsConfig* GetSafetyTipsRemoteConfigProto() {
  return SafetyTipsConfigSingleton::GetInstance().GetProto();
}

bool IsUrlAllowlistedBySafetyTipsComponent(
    const reputation::SafetyTipsConfig* proto,
    const GURL& visited_url,
    const GURL& canonical_url) {
  DCHECK(proto);
  DCHECK(visited_url.is_valid());
  std::vector<std::string> patterns;
  UrlToSafetyTipPatterns(visited_url, &patterns);
  auto allowed_patterns = proto->allowed_pattern();
  for (const auto& pattern : patterns) {
    reputation::UrlPattern search_target;
    search_target.set_pattern(pattern);

    auto maybe_before = std::lower_bound(
        allowed_patterns.begin(), allowed_patterns.end(), search_target,
        [](const reputation::UrlPattern& a, const reputation::UrlPattern& b)
            -> bool { return a.pattern() < b.pattern(); });

    if (maybe_before != allowed_patterns.end() &&
        pattern == maybe_before->pattern()) {
      // If no cohorts are given, it's a universal allowlist entry.
      if (maybe_before->cohort_index_size() == 0) {
        return true;
      }

      for (const unsigned cohort_index : maybe_before->cohort_index()) {
        if (IsUrlAllowedByCohort(proto, canonical_url, cohort_index)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool IsTargetHostAllowlistedBySafetyTipsComponent(
    const reputation::SafetyTipsConfig* proto,
    const std::string& hostname) {
  DCHECK(!hostname.empty());
  if (proto == nullptr) {
    return false;
  }
  for (const auto& host_pattern : proto->allowed_target_pattern()) {
    if (!host_pattern.has_regex()) {
      continue;
    }
    DCHECK(!host_pattern.regex().empty());
    const re2::RE2 regex(host_pattern.regex());
    DCHECK(regex.ok());
    if (re2::RE2::FullMatch(hostname, regex)) {
      return true;
    }
  }
  return false;
}

bool IsCommonWordInConfigProto(const reputation::SafetyTipsConfig* proto,
                               const std::string& word) {
  // proto is nullptr when running in non-Lookalike tests.
  if (proto == nullptr) {
    return false;
  }

  auto common_words = proto->common_word();
  DCHECK(base::ranges::is_sorted(common_words.begin(), common_words.end()));
  auto lower = std::lower_bound(
      common_words.begin(), common_words.end(), word,
      [](const std::string& a, const std::string& b) -> bool { return a < b; });

  return lower != common_words.end() && word == *lower;
}

}  // namespace lookalikes
