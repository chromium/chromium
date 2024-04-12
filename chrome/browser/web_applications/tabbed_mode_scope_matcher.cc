// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/tabbed_mode_scope_matcher.h"

#include <utility>

#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/pattern.h"

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

bool MatchScopePart(std::optional<RE2::Set>& scope_set,
                    const std::string& part) {
  if (scope_set.has_value()) {
    return scope_set->Match(part, nullptr);
  }
  return true;
}

}  // anonymous namespace

namespace web_app {

TabbedModeScopeMatcher::TabbedModeScopeMatcher(
    const blink::SafeUrlPattern& pattern)
    : protocol_scope_set_(MakeScopeSet(pattern.protocol)),
      username_scope_set_(MakeScopeSet(pattern.username)),
      password_scope_set_(MakeScopeSet(pattern.password)),
      hostname_scope_set_(MakeScopeSet(pattern.hostname)),
      port_scope_set_(MakeScopeSet(pattern.port)),
      pathname_scope_set_(MakeScopeSet(pattern.pathname)),
      search_scope_set_(MakeScopeSet(pattern.search)),
      hash_scope_set_(MakeScopeSet(pattern.hash)) {}

TabbedModeScopeMatcher::~TabbedModeScopeMatcher() = default;

TabbedModeScopeMatcher::TabbedModeScopeMatcher(TabbedModeScopeMatcher&& other) =
    default;
TabbedModeScopeMatcher& TabbedModeScopeMatcher::operator=(
    TabbedModeScopeMatcher&& other) = default;

bool TabbedModeScopeMatcher::Match(const GURL& url) {
  return MatchScopePart(protocol_scope_set_, url.scheme()) &&
         MatchScopePart(username_scope_set_, url.username()) &&
         MatchScopePart(password_scope_set_, url.password()) &&
         MatchScopePart(hostname_scope_set_, url.host()) &&
         MatchScopePart(port_scope_set_, url.port()) &&
         MatchScopePart(pathname_scope_set_, url.path()) &&
         MatchScopePart(search_scope_set_, url.query()) &&
         MatchScopePart(hash_scope_set_, url.ref());
}

}  // namespace web_app
