// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TABBED_MODE_SCOPE_MATCHER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TABBED_MODE_SCOPE_MATCHER_H_

#include <optional>

#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/re2/src/re2/set.h"
#include "url/gurl.h"

namespace web_app {

// A helper class to abstract the process of matching URLs against a home tab
// scope (communicated from the renderer process via blink::SafeUrlPattern) for
// tabbed mode.
class TabbedModeScopeMatcher {
 public:
  explicit TabbedModeScopeMatcher(const blink::SafeUrlPattern& pattern);
  ~TabbedModeScopeMatcher();

  TabbedModeScopeMatcher(TabbedModeScopeMatcher&& other);
  TabbedModeScopeMatcher& operator=(TabbedModeScopeMatcher&& other);

  TabbedModeScopeMatcher(const TabbedModeScopeMatcher&) = delete;
  TabbedModeScopeMatcher& operator=(const TabbedModeScopeMatcher&) = delete;

  bool Match(const GURL& url);

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

  // A compiled RE2::Set to match portmembers, or nullopt if there is no
  // matching required for protocol.
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

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TABBED_MODE_SCOPE_MATCHER_H_
