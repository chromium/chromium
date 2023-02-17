// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"

#include "base/functional/overloaded.h"
#include "base/json/values_util.h"
#include "base/values.h"

namespace web_app {

bool InstalledBundle::operator==(const InstalledBundle& other) const {
  return path == other.path;
}
bool InstalledBundle::operator!=(const InstalledBundle& other) const {
  return !(*this == other);
}

bool DevModeBundle::operator==(const DevModeBundle& other) const {
  return path == other.path;
}
bool DevModeBundle::operator!=(const DevModeBundle& other) const {
  return !(*this == other);
}

bool DevModeProxy::operator==(const DevModeProxy& other) const {
  return proxy_url == other.proxy_url;
}
bool DevModeProxy::operator!=(const DevModeProxy& other) const {
  return !(*this == other);
}

base::Value IsolatedWebAppLocationAsDebugValue(
    const IsolatedWebAppLocation& location) {
  base::Value::Dict value;
  absl::visit(base::Overloaded{
                  [&value](const InstalledBundle& bundle) {
                    value.SetByDottedPath("installed_bundle.path",
                                          base::FilePathToValue(bundle.path));
                  },
                  [&value](const DevModeBundle& bundle) {
                    value.SetByDottedPath("dev_mode_bundle.path",
                                          base::FilePathToValue(bundle.path));
                  },
                  [&value](const DevModeProxy& proxy) {
                    DCHECK(!proxy.proxy_url.opaque());
                    value.SetByDottedPath("dev_mode_proxy.proxy_url",
                                          proxy.proxy_url.GetDebugString());
                  },
              },
              location);
  return base::Value(std::move(value));
}

}  // namespace web_app
