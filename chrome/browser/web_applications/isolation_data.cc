// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolation_data.h"

#include "base/functional/overloaded.h"
#include "base/json/values_util.h"

namespace web_app {

IsolationData::IsolationData(
    absl::variant<InstalledBundle, DevModeBundle, DevModeProxy> content)
    : content(content) {}
IsolationData::~IsolationData() = default;
IsolationData::IsolationData(const IsolationData&) = default;
IsolationData& IsolationData::operator=(const IsolationData&) = default;
IsolationData::IsolationData(IsolationData&&) = default;
IsolationData& IsolationData::operator=(IsolationData&&) = default;

bool IsolationData::operator==(const IsolationData& other) const {
  return content == other.content;
}
bool IsolationData::operator!=(const IsolationData& other) const {
  return !(*this == other);
}

bool IsolationData::InstalledBundle::operator==(
    const IsolationData::InstalledBundle& other) const {
  return path == other.path;
}
bool IsolationData::InstalledBundle::operator!=(
    const IsolationData::InstalledBundle& other) const {
  return !(*this == other);
}

bool IsolationData::DevModeBundle::operator==(
    const IsolationData::DevModeBundle& other) const {
  return path == other.path;
}
bool IsolationData::DevModeBundle::operator!=(
    const IsolationData::DevModeBundle& other) const {
  return !(*this == other);
}

bool IsolationData::DevModeProxy::operator==(
    const IsolationData::DevModeProxy& other) const {
  return proxy_url == other.proxy_url;
}
bool IsolationData::DevModeProxy::operator!=(
    const IsolationData::DevModeProxy& other) const {
  return !(*this == other);
}

base::Value IsolationData::AsDebugValue() const {
  base::Value::Dict value;
  absl::visit(base::Overloaded{
                  [&value](const IsolationData::InstalledBundle& bundle) {
                    value.SetByDottedPath("content.installed_bundle.path",
                                          base::FilePathToValue(bundle.path));
                  },
                  [&value](const IsolationData::DevModeBundle& bundle) {
                    value.SetByDottedPath("content.dev_mode_bundle.path",
                                          base::FilePathToValue(bundle.path));
                  },
                  [&value](const IsolationData::DevModeProxy& proxy) {
                    DCHECK(!proxy.proxy_url.opaque());
                    value.SetByDottedPath("content.dev_mode_proxy.proxy_url",
                                          proxy.proxy_url.GetDebugString());
                  },
              },
              content);
  return base::Value(std::move(value));
}

}  // namespace web_app
