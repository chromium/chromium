// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_URL_PATTERN_WITH_REGEX_MATCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_URL_PATTERN_WITH_REGEX_MATCHER_H_

#include <optional>

#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/set.h"
#include "url/gurl.h"

namespace web_app {

// A helper class to abstract the process of matching URLs against a
// `blink::SafeUrlPattern`, considering regular expression matching as well.
// Useful for matching certain fields in a web app like the `home_tab_scope`
// and `borderless_url_patterns`.
class UrlPatternWithRegexMatcher {
 public:
  explicit UrlPatternWithRegexMatcher(const blink::SafeUrlPattern& pattern);
  ~UrlPatternWithRegexMatcher();

  UrlPatternWithRegexMatcher(UrlPatternWithRegexMatcher&& other);
  UrlPatternWithRegexMatcher& operator=(UrlPatternWithRegexMatcher&& other);

  UrlPatternWithRegexMatcher(const UrlPatternWithRegexMatcher&) = delete;
  UrlPatternWithRegexMatcher& operator=(const UrlPatternWithRegexMatcher&) =
      delete;

  bool Match(const GURL& url) const;

 private:
  // A compiled RE2::Set to match protocol members, or nullopt if there is no
  // matching required for protocol.
  std::optional<RE2::Set> protocol_scope_set_;

  // A compiled RE2::Set to match username members, or nullopt if there is no
  // matching required for username.
  std::optional<RE2::Set> username_scope_set_;

  // A compiled RE2::Set to match password members, or nullopt if there is no
  // matching required for password.
  std::optional<RE2::Set> password_scope_set_;

  // A compiled RE2::Set to match hostname members, or nullopt if there is no
  // matching required for hostname.
  std::optional<RE2::Set> hostname_scope_set_;

  // A compiled RE2::Set to match port members, or nullopt if there is no
  // matching required for port.
  std::optional<RE2::Set> port_scope_set_;

  // A compiled RE2::Set to match pathname members, or nullopt if there is no
  // matching required for pathname.
  std::optional<RE2::Set> pathname_scope_set_;

  // A compiled RE2::Set to match search members, or nullopt if there is no
  // matching required for search.
  std::optional<RE2::Set> search_scope_set_;

  // A compiled RE2::Set to match hash members, or nullopt if there is no
  // matching required for hash.
  std::optional<RE2::Set> hash_scope_set_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_URL_PATTERN_WITH_REGEX_MATCHER_H_
