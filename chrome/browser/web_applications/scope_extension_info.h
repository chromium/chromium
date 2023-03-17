// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/values.h"
#include "url/origin.h"

namespace web_app {

// Contains information about a web app's scope extension information derived
// from its web app manifest.
struct ScopeExtensionInfo {
  ScopeExtensionInfo() = default;
  explicit ScopeExtensionInfo(const url::Origin& origin);
  ScopeExtensionInfo(const url::Origin& origin, bool has_origin_wildcard);

  // Copyable to support web_app::WebApp being copyable as it has a
  // ScopeExtensions member variable.
  ScopeExtensionInfo(const ScopeExtensionInfo&) = default;
  ScopeExtensionInfo& operator=(const ScopeExtensionInfo&) = default;
  // Movable to support being contained in std::vector, which requires value
  // types to be copyable or movable.
  ScopeExtensionInfo(ScopeExtensionInfo&&) = default;
  ScopeExtensionInfo& operator=(ScopeExtensionInfo&&) = default;

  ~ScopeExtensionInfo() = default;

  // Reset the scope extension to its default state.
  REINITIALIZES_AFTER_MOVE void Reset();

  base::Value AsDebugValue() const;

  url::Origin origin;

  bool has_origin_wildcard = false;
};

bool operator==(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2);

bool operator!=(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2);

// Allow ScopeExtensionInfo to be used as a key in STL (for example, a std::set
// or std::map).
bool operator<(const ScopeExtensionInfo& scope_extension1,
               const ScopeExtensionInfo& scope_extension2);

using ScopeExtensions = std::vector<ScopeExtensionInfo>;
using ScopeExtensionMap = std::unordered_map<std::string, ScopeExtensionInfo>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCOPE_EXTENSION_INFO_H_
