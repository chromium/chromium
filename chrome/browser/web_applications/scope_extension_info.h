// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_

#include <string>
#include <unordered_map>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace proto {
class WebAppScopeExtension;
}  // namespace proto

// Contains information about a web app's scope extension information derived
// from its web app manifest.
struct ScopeExtensionInfo {
  ScopeExtensionInfo() = delete;
  ScopeExtensionInfo(const ScopeExtensionInfo&) = default;
  ScopeExtensionInfo& operator=(const ScopeExtensionInfo&) = default;

  friend bool operator==(const ScopeExtensionInfo&,
                         const ScopeExtensionInfo&) = default;

  friend auto operator<=>(const ScopeExtensionInfo&,
                          const ScopeExtensionInfo&) = default;

  static ScopeExtensionInfo CreateForOrigin(url::Origin origin,
                                            bool has_origin_wildcard = false);
  static ScopeExtensionInfo CreateForScope(GURL scope,
                                           bool has_origin_wildcard = false);
  // Used specifically in WebAppDatabase::CreateWebAppProto
  static ScopeExtensionInfo CreateForProto(
      const proto::WebAppScopeExtension& web_app_scope_extension_proto);

  // Reset the scope extension to its default state.
  REINITIALIZES_AFTER_MOVE void Reset();

  base::Value AsDebugValue() const;

  url::Origin origin;

  // Must be the same origin as `origin` else CHECK-fails.
  // `scope` will also drop any queries or fragments from the URL per manifest
  // scope rules: https://w3c.github.io/manifest/#scope-member
  GURL scope;

  bool has_origin_wildcard = false;

 private:
  ScopeExtensionInfo(url::Origin origin, GURL scope, bool has_origin_wildcard);
};

using ScopeExtensions = base::flat_set<ScopeExtensionInfo>;
using ScopeExtensionMap = std::unordered_map<std::string, ScopeExtensionInfo>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_
