// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/intent_filter.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "url/gurl.h"

namespace arc {

IntentFilter::IntentFilter() = default;
IntentFilter::IntentFilter(IntentFilter&& other) = default;

IntentFilter::IntentFilter(
    const std::string& package_name,
    std::vector<IntentFilter::AuthorityEntry> authorities,
    std::vector<IntentFilter::PatternMatcher> paths,
    std::vector<std::string> schemes)
    : package_name_(package_name),
      authorities_(std::move(authorities)),
      schemes_(std::move(schemes)) {
  // In order to register a path we need to have at least one authority.
  if (!authorities_.empty())
    paths_ = std::move(paths);
}

IntentFilter::~IntentFilter() = default;

IntentFilter& IntentFilter::operator=(IntentFilter&& other) = default;

// Logically, this maps to IntentFilter#match, but this code only deals with
// view intents for http/https URLs and so it really only implements the
// #matchData part of the match code.
bool IntentFilter::Match(const GURL& url) const {
  // Chrome-side code only receives view intents for http/https URLs, so this
  // match code really only implements the matchData part of the android
  // IntentFilter class.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Match the authority and the path. If there are no authorities for this
  // filter, we can treat this as a match, since we already know this filter
  // has a http(s) scheme and it doesn't corresponds to a MIME type.
  if (!authorities_.empty()) {
    return MatchDataAuthority(url) && (paths_.empty() || HasDataPath(url));
  }

  return true;
}

// Transcribed from android's IntentFilter#hasDataPath.
bool IntentFilter::HasDataPath(const GURL& url) const {
  const std::string path = url.path();
  for (const PatternMatcher& pattern : paths_) {
    if (pattern.Match(path)) {
      return true;
    }
  }
  return false;
}

// Transcribed from android's IntentFilter#matchDataAuthority.
bool IntentFilter::MatchDataAuthority(const GURL& url) const {
  for (const AuthorityEntry& authority : authorities_) {
    if (authority.Match(url)) {
      return true;
    }
  }
  return false;
}

IntentFilter::AuthorityEntry::AuthorityEntry() = default;
IntentFilter::AuthorityEntry::AuthorityEntry(
    IntentFilter::AuthorityEntry&& other) = default;

IntentFilter::AuthorityEntry& IntentFilter::AuthorityEntry::operator=(
    IntentFilter::AuthorityEntry&& other) = default;

IntentFilter::AuthorityEntry::AuthorityEntry(const std::string& host, int port)
    : host_(host), port_(port) {
  // Wildcards are only allowed at the front of the host string.
  wild_ = !host_.empty() && host_[0] == '*';
  if (wild_) {
    host_ = host_.substr(1);
  }

  // TODO(kenobi): Not i18n-friendly.  Figure out how to correctly deal with
  // IDNs.
  host_ = base::ToLowerASCII(host_);
}

// Transcribed from android's IntentFilter.AuthorityEntry#match.
bool IntentFilter::AuthorityEntry::Match(const GURL& url) const {
  if (!url.has_host()) {
    return false;
  }

  // Note: On android, intent filters with explicit port specifications only
  // match URLs with explict ports, even if the specified port is the default
  // port.  Using GURL::EffectiveIntPort instead of GURL::IntPort means that
  // this code differs in behaviour (i.e. it just matches the effective port,
  // ignoring whether it was implicitly or explicitly specified).
  //
  // We do this because it provides an optimistic match - ensuring that the
  // disambiguation code doesn't miss URLs that might be handled by android
  // apps.  This doesn't cause misrouted intents because this check is followed
  // up by a mojo call that actually verifies the list of packages that could
  // accept the given intent.
  if (port_ >= 0 && port_ != url.EffectiveIntPort()) {
    return false;
  }

  if (wild_) {
    return base::EndsWith(url.host_piece(), host_,
                          base::CompareCase::INSENSITIVE_ASCII);
  }
  // TODO(kenobi): Not i18n-friendly.  Figure out how to correctly deal with
  // IDNs.
  return host_ == base::ToLowerASCII(url.host_piece());
}

IntentFilter::PatternMatcher::PatternMatcher() = default;
IntentFilter::PatternMatcher::PatternMatcher(
    IntentFilter::PatternMatcher&& other) = default;

IntentFilter::PatternMatcher::PatternMatcher(const std::string& pattern,
                                             mojom::PatternType match_type)
    : pattern_(pattern), match_type_(match_type) {}

IntentFilter::PatternMatcher& IntentFilter::PatternMatcher::operator=(
    IntentFilter::PatternMatcher&& other) = default;

// Transcribed from android's PatternMatcher#matchPattern.
bool IntentFilter::PatternMatcher::Match(const std::string& str) const {
  if (str.empty()) {
    return false;
  }
  switch (match_type_) {
    case mojom::PatternType::PATTERN_LITERAL:
      return str == pattern_;
    case mojom::PatternType::PATTERN_PREFIX:
      return base::StartsWith(str, pattern_,
                              base::CompareCase::INSENSITIVE_ASCII);
    case mojom::PatternType::PATTERN_SIMPLE_GLOB:
      return apps_util::MatchGlob(str, pattern_);
  }

  return false;
}

}  // namespace arc
