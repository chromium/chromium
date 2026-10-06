// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/safety_tips_config.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/no_destructor.h"
#include "components/lookalikes/core/flat_safety_tips_allowlist.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using safe_browsing::SBProtocolManagerUtil;

namespace lookalikes {

namespace {

using AllowedTargetRegexes = std::vector<std::unique_ptr<re2::RE2>>;

// Compiles the allowed target regexes of |proto|. Regexes that fail to compile
// are dropped, since they can't match anything.
AllowedTargetRegexes CompileAllowedTargetRegexes(
    const reputation::SafetyTipsConfig& proto) {
  AllowedTargetRegexes regexes;
  regexes.reserve(proto.allowed_target_pattern_size());
  for (const auto& host_pattern : proto.allowed_target_pattern()) {
    if (!host_pattern.has_regex()) {
      continue;
    }
    auto regex = std::make_unique<re2::RE2>(host_pattern.regex());
    if (regex->ok()) {
      regexes.push_back(std::move(regex));
    }
  }
  return regexes;
}

// Returns whether |hostname| fully matches any of |regexes|.
bool MatchesAnyRegex(const AllowedTargetRegexes& regexes,
                     const std::string& hostname) {
  return std::ranges::any_of(
      regexes, [&hostname](const std::unique_ptr<re2::RE2>& regex) {
        return re2::RE2::FullMatch(hostname, *regex);
      });
}

class SafetyTipsConfigSingleton {
 public:
  void SetConfig(std::unique_ptr<reputation::SafetyTipsConfig> proto,
                 std::unique_ptr<FlatSafetyTipsAllowlist> allowlist) {
    allowed_target_regexes_.reset();
    allowlist_ = std::move(allowlist);
    proto_ = std::move(proto);
  }

  reputation::SafetyTipsConfig* GetProto() const { return proto_.get(); }

  // Returns the URL allowlist moved out of the installed config, or nullptr if
  // no config is installed.
  const FlatSafetyTipsAllowlist* GetAllowlist() const {
    return allowlist_.get();
  }

  // Returns the allowed target regexes of the installed config, compiling them
  // on first use. Must only be called while a config is installed.
  const AllowedTargetRegexes& GetAllowedTargetRegexes() {
    DCHECK(proto_);
    if (!allowed_target_regexes_) {
      allowed_target_regexes_ = CompileAllowedTargetRegexes(*proto_);
    }
    return *allowed_target_regexes_;
  }

  static SafetyTipsConfigSingleton& GetInstance() {
    static base::NoDestructor<SafetyTipsConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<reputation::SafetyTipsConfig> proto_;
  // The URL allowlist moved out of |proto_|. Null if and only if |proto_| is.
  std::unique_ptr<FlatSafetyTipsAllowlist> allowlist_;
  // Compiled from |proto_| once, rather than on every lookup.
  std::optional<AllowedTargetRegexes> allowed_target_regexes_;
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
  SBProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.GetHost());
  } else {
    SBProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  SBProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
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
  std::unique_ptr<FlatSafetyTipsAllowlist> allowlist;
  if (proto) {
    allowlist = FlatSafetyTipsAllowlist::ExtractFrom(*proto);
  }
  SetSafetyTipsRemoteConfig(std::move(proto), std::move(allowlist));
}

void SetSafetyTipsRemoteConfig(
    std::unique_ptr<reputation::SafetyTipsConfig> proto,
    std::unique_ptr<FlatSafetyTipsAllowlist> allowlist) {
  CHECK_EQ(!proto, !allowlist);
  CHECK(!proto ||
        (proto->allowed_pattern().empty() &&
         proto->canonical_pattern().empty() && proto->cohort().empty()));
  SafetyTipsConfigSingleton::GetInstance().SetConfig(std::move(proto),
                                                     std::move(allowlist));
}

const reputation::SafetyTipsConfig* GetSafetyTipsRemoteConfigProto() {
  return SafetyTipsConfigSingleton::GetInstance().GetProto();
}

const FlatSafetyTipsAllowlist* GetSafetyTipsRemoteAllowlistForTesting() {
  return SafetyTipsConfigSingleton::GetInstance().GetAllowlist();
}

bool IsUrlAllowlistedBySafetyTipsComponent(
    const reputation::SafetyTipsConfig* proto,
    const GURL& visited_url,
    const GURL& canonical_url) {
  DCHECK(proto);
  DCHECK(visited_url.is_valid());
  std::vector<std::string> patterns;
  UrlToSafetyTipPatterns(visited_url, &patterns);

  // The allowlist of the installed config was moved out of it. Other configs
  // (e.g. in tests) still hold theirs.
  const SafetyTipsConfigSingleton& singleton =
      SafetyTipsConfigSingleton::GetInstance();
  if (proto == singleton.GetProto()) {
    return singleton.GetAllowlist()->IsUrlAllowlisted(
        patterns, [&canonical_url] {
          DCHECK(canonical_url.is_valid());
          std::vector<std::string> canonical_patterns;
          UrlToSafetyTipPatterns(canonical_url, &canonical_patterns);
          return canonical_patterns;
        });
  }

  const auto& allowed_patterns = proto->allowed_pattern();
  for (const auto& pattern : patterns) {
    auto maybe_before = std::lower_bound(
        allowed_patterns.begin(), allowed_patterns.end(), pattern,
        [](const reputation::UrlPattern& a, const std::string& b) -> bool {
          return a.pattern() < b;
        });

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
  SafetyTipsConfigSingleton& singleton =
      SafetyTipsConfigSingleton::GetInstance();
  if (proto == singleton.GetProto()) {
    return MatchesAnyRegex(singleton.GetAllowedTargetRegexes(), hostname);
  }
  // Configs other than the installed one (e.g. in tests) are compiled for this
  // call only.
  return MatchesAnyRegex(CompileAllowedTargetRegexes(*proto), hostname);
}

bool IsCommonWordInConfigProto(const reputation::SafetyTipsConfig* proto,
                               const std::string& word) {
  // proto is nullptr when running in non-Lookalike tests.
  if (proto == nullptr) {
    return false;
  }

  const auto& common_words = proto->common_word();
  DCHECK(std::ranges::is_sorted(common_words.begin(), common_words.end()));
  auto lower = std::lower_bound(
      common_words.begin(), common_words.end(), word,
      [](const std::string& a, const std::string& b) -> bool { return a < b; });

  return lower != common_words.end() && word == *lower;
}

}  // namespace lookalikes
