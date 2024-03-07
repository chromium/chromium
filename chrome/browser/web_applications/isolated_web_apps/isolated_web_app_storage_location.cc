// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"

#include <string>

#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IwaStorageOwnedBundle::IwaStorageOwnedBundle(std::string dir_name_ascii,
                                             bool dev_mode)
    : dir_name_ascii_(std::move(dir_name_ascii)), dev_mode_(dev_mode) {}
IwaStorageOwnedBundle::~IwaStorageOwnedBundle() = default;

bool IwaStorageOwnedBundle::operator==(
    const IwaStorageOwnedBundle& other) const = default;

base::FilePath IwaStorageOwnedBundle::GetPath(
    const base::FilePath profile_dir) const {
  return profile_dir.Append(kIwaDirName)
      .AppendASCII(dir_name_ascii_)
      .Append(kMainSwbnFileName);
}

base::Value IwaStorageOwnedBundle::ToDebugValue() const {
  return base::Value(base::Value::Dict()
                         .Set("dir_name_ascii", dir_name_ascii_)
                         .Set("dev_mode", dev_mode_));
}

std::ostream& operator<<(std::ostream& os, IwaStorageOwnedBundle location) {
  return os << location.ToDebugValue();
}

IwaStorageUnownedBundle::IwaStorageUnownedBundle(base::FilePath path)
    : path_(std::move(path)) {}
IwaStorageUnownedBundle::~IwaStorageUnownedBundle() = default;

bool IwaStorageUnownedBundle::operator==(
    const IwaStorageUnownedBundle& other) const = default;

base::Value IwaStorageUnownedBundle::ToDebugValue() const {
  return base::Value(
      base::Value::Dict().Set("path", base::FilePathToValue(path_)));
}

std::ostream& operator<<(std::ostream& os, IwaStorageUnownedBundle location) {
  return os << location.ToDebugValue();
}

IwaStorageProxy::IwaStorageProxy(url::Origin proxy_url)
    : proxy_url_(std::move(proxy_url)) {}
IwaStorageProxy::~IwaStorageProxy() = default;

bool IwaStorageProxy::operator==(const IwaStorageProxy& other) const = default;

base::Value IwaStorageProxy::ToDebugValue() const {
  return base::Value(
      base::Value::Dict().Set("proxy_url", proxy_url_.GetDebugString()));
}

std::ostream& operator<<(std::ostream& os, IwaStorageProxy location) {
  return os << location.ToDebugValue();
}

IsolatedWebAppStorageLocation::IsolatedWebAppStorageLocation(
    const IsolatedWebAppStorageLocation&) = default;
IsolatedWebAppStorageLocation& IsolatedWebAppStorageLocation::operator=(
    const IsolatedWebAppStorageLocation&) = default;

IsolatedWebAppStorageLocation::~IsolatedWebAppStorageLocation() = default;

bool IsolatedWebAppStorageLocation::operator==(
    const IsolatedWebAppStorageLocation& other) const = default;

IsolatedWebAppLocation IsolatedWebAppStorageLocation::ToLocationDeprecated(
    const base::FilePath& profile_dir) const {
  return absl::visit(
      base::Overloaded{
          [&](const IwaStorageOwnedBundle& bundle) -> IsolatedWebAppLocation {
            base::FilePath path = bundle.GetPath(profile_dir);
            if (bundle.dev_mode()) {
              return DevModeBundle{.path = path};
            }
            return InstalledBundle{.path = path};
          },
          [&](const IwaStorageUnownedBundle& bundle) -> IsolatedWebAppLocation {
            return DevModeBundle{.path = bundle.path()};
          },
          [&](const IwaStorageProxy& proxy) -> IsolatedWebAppLocation {
            return DevModeProxy{.proxy_url = proxy.proxy_url()};
          },
      },
      variant_);
}

bool IsolatedWebAppStorageLocation::dev_mode() const {
  return absl::visit(base::Overloaded{[](const auto& location) {
                       return location.dev_mode();
                     }},
                     variant_);
}

base::Value IsolatedWebAppStorageLocation::ToDebugValue() const {
  base::Value::Dict value;
  absl::visit(base::Overloaded{
                  [&value](const OwnedBundle& bundle) {
                    value.Set("owned_bundle", bundle.ToDebugValue());
                  },
                  [&value](const UnownedBundle& bundle) {
                    value.Set("unowned_bundle", bundle.ToDebugValue());
                  },
                  [&value](const Proxy& proxy) {
                    value.Set("proxy", proxy.ToDebugValue());
                  },
              },
              variant_);
  return base::Value(std::move(value));
}

absl::variant<IwaSourceBundle, IwaSourceProxy>
IsolatedWebAppStorageLocation::ToSource(
    const base::FilePath& profile_dir) const {
  return absl::visit(
      base::Overloaded{
          [&profile_dir](const OwnedBundle& location)
              -> absl::variant<IwaSourceBundle, IwaSourceProxy> {
            return IwaSourceBundle{.path = location.GetPath(profile_dir)};
          },
          [](const UnownedBundle& location)
              -> absl::variant<IwaSourceBundle, IwaSourceProxy> {
            return IwaSourceBundle{.path = location.path()};
          },
          [](const Proxy& location)
              -> absl::variant<IwaSourceBundle, IwaSourceProxy> {
            return IwaSourceProxy{.proxy_url = location.proxy_url()};
          },
      },
      variant_);
}

std::ostream& operator<<(std::ostream& os,
                         IsolatedWebAppStorageLocation location) {
  return os << location.ToDebugValue();
}

}  // namespace web_app
