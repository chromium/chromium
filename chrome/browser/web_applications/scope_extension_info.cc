// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "chrome/browser/web_applications/scope_extension_info.h"

namespace web_app {

base::Value ScopeExtensionInfo::AsDebugValue() const {
  base::Value::Dict root = base::Value::Dict()
                               .Set("origin", origin.GetDebugString())
                               .Set("has_origin_wildcard", has_origin_wildcard);
  return base::Value(std::move(root));
}

void ScopeExtensionInfo::Reset() {
  origin = url::Origin();
  has_origin_wildcard = false;
}

bool operator==(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2) {
  return scope_extension1.origin == scope_extension2.origin &&
         scope_extension1.has_origin_wildcard ==
             scope_extension2.has_origin_wildcard;
}

bool operator!=(const ScopeExtensionInfo& scope_extension1,
                const ScopeExtensionInfo& scope_extension2) {
  return !(scope_extension1 == scope_extension2);
}

bool operator<(const ScopeExtensionInfo& scope_extension1,
               const ScopeExtensionInfo& scope_extension2) {
  return std::tie(scope_extension1.origin,
                  scope_extension1.has_origin_wildcard) <
         std::tie(scope_extension2.origin,
                  scope_extension2.has_origin_wildcard);
}

}  // namespace web_app
