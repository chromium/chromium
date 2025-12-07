// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/url_pattern_with_regex_matcher.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace {

std::optional<RE2::Set> MakeScopeSet(
    const std::vector<liburlpattern::Part>& part_list) {
  if (part_list.empty()) {
    return std::nullopt;
  }

  RE2::Set scope_set = RE2::Set(RE2::Options(), RE2::Anchor::UNANCHORED);
  liburlpattern::Options options = {.delimiter_list = "/",
                                    .prefix_list = "/",
                                    .sensitive = true,
                                    .strict = false};
  liburlpattern::Pattern pattern(part_list, options, "[^/]+?");
  std::string error;
  scope_set.Add(pattern.GenerateRegexString(), &error);

  if (scope_set.Compile()) {
    return std::make_optional<RE2::Set>(std::move(scope_set));
  }

  return std::nullopt;
}

bool MatchScopePart(const std::optional<RE2::Set>& scope_set,
                    const std::string& part) {
  if (scope_set.has_value()) {
    return scope_set->Match(part, nullptr);
  }
  return true;
}

}  // anonymous namespace

namespace web_app {

UrlPatternWithRegexMatcher::UrlPatternWithRegexMatcher(
    const blink::SafeUrlPattern& pattern)
    : protocol_scope_set_(MakeScopeSet(pattern.protocol)),
      username_scope_set_(MakeScopeSet(pattern.username)),
      password_scope_set_(MakeScopeSet(pattern.password)),
      hostname_scope_set_(MakeScopeSet(pattern.hostname)),
      port_scope_set_(MakeScopeSet(pattern.port)),
      pathname_scope_set_(MakeScopeSet(pattern.pathname)),
      search_scope_set_(MakeScopeSet(pattern.search)),
      hash_scope_set_(MakeScopeSet(pattern.hash)) {}

UrlPatternWithRegexMatcher::~UrlPatternWithRegexMatcher() = default;

UrlPatternWithRegexMatcher::UrlPatternWithRegexMatcher(
    UrlPatternWithRegexMatcher&& other) = default;
UrlPatternWithRegexMatcher& UrlPatternWithRegexMatcher::operator=(
    UrlPatternWithRegexMatcher&& other) = default;

bool UrlPatternWithRegexMatcher::Match(const GURL& url) const {
  return MatchScopePart(protocol_scope_set_, url.GetScheme()) &&
         MatchScopePart(username_scope_set_, url.GetUsername()) &&
         MatchScopePart(password_scope_set_, url.GetPassword()) &&
         MatchScopePart(hostname_scope_set_, url.GetHost()) &&
         MatchScopePart(port_scope_set_, url.GetPort()) &&
         MatchScopePart(pathname_scope_set_, url.GetPath()) &&
         MatchScopePart(search_scope_set_, url.GetQuery()) &&
         MatchScopePart(hash_scope_set_, url.GetRef());
}

}  // namespace web_app
