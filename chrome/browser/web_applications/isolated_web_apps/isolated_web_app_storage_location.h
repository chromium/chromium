// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_STORAGE_LOCATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_STORAGE_LOCATION_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace web_app {

inline constexpr base::FilePath::CharType kIwaDirName[] =
    FILE_PATH_LITERAL("iwa");
inline constexpr base::FilePath::CharType kMainSwbnFileName[] =
    FILE_PATH_LITERAL("main.swbn");

// An IWA that is stored as a bundle that is managed and owned by the browser.
// It is located in the profile directory.
class IwaStorageOwnedBundle {
 public:
  IwaStorageOwnedBundle(std::string dir_name_ascii, bool dev_mode);
  ~IwaStorageOwnedBundle();

  bool operator==(const IwaStorageOwnedBundle& other) const;

  const std::string& dir_name_ascii() const { return dir_name_ascii_; }
  bool dev_mode() const { return dev_mode_; }

  base::FilePath GetPath(const base::FilePath profile_dir) const;

  base::Value ToDebugValue() const;

 private:
  std::string dir_name_ascii_;
  bool dev_mode_;
};

std::ostream& operator<<(std::ostream& os, IwaStorageOwnedBundle location);

// An IWA that is stored as a bundle that is not owned by the browser. It must
// never be touched (even when uninstalling) and is always located outside of
// the profile directory.
class IwaStorageUnownedBundle {
 public:
  explicit IwaStorageUnownedBundle(base::FilePath path);
  ~IwaStorageUnownedBundle();

  bool operator==(const IwaStorageUnownedBundle& other) const;

  const base::FilePath& path() const { return path_; }
  bool dev_mode() const { return true; }

  base::Value ToDebugValue() const;

 private:
  base::FilePath path_;
};

std::ostream& operator<<(std::ostream& os, IwaStorageUnownedBundle location);

// An IWA whose source is a virtual bundle served through an HTTP proxy.
//
// The proxy origin must never be opaque.
class IwaStorageProxy {
 public:
  explicit IwaStorageProxy(url::Origin proxy_url);
  ~IwaStorageProxy();

  bool operator==(const IwaStorageProxy& other) const;

  const url::Origin& proxy_url() const { return proxy_url_; }
  bool dev_mode() const { return true; }

  base::Value ToDebugValue() const;

 private:
  url::Origin proxy_url_;
};

std::ostream& operator<<(std::ostream& os, IwaStorageProxy location);

// Represents how the IWA is stored, and is persisted to the Web App database.
// As such, this class should strive to remain a simple wrapper around the
// storage representation in the database. Only code directly related to
// install, update, and serving from a bundle/proxy should have to deal with
// this. All other code should use the higher-level abstractions defined in
// `isolated_web_app_source.h`.
class IsolatedWebAppStorageLocation {
 public:
  using OwnedBundle = IwaStorageOwnedBundle;
  using UnownedBundle = IwaStorageUnownedBundle;
  using Proxy = IwaStorageProxy;

  using Variant = absl::variant<OwnedBundle, UnownedBundle, Proxy>;

  // Create implicit constructors for each of the variant values. This allows
  // passing an instance of one of the variant values (e.g., `OwnedBundle`) to a
  // function accepting an `IsolatedWebAppStorageLocation` without an explicit
  // conversion. Thus, this class behaves more akin to how a "raw"
  // `absl::variant` would behave, and reduces the amount of boilerplate
  // necessary.
  template <typename V>
  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ IsolatedWebAppStorageLocation(V variant_value)
    requires(std::is_constructible_v<Variant, V>)
      : variant_(std::move(variant_value)) {}

  IsolatedWebAppStorageLocation(const IsolatedWebAppStorageLocation&);
  IsolatedWebAppStorageLocation& operator=(
      const IsolatedWebAppStorageLocation&);

  ~IsolatedWebAppStorageLocation();

  bool operator==(const IsolatedWebAppStorageLocation& other) const;

  const Variant& variant() const { return variant_; }
  bool dev_mode() const;

  base::Value ToDebugValue() const;

 private:
  Variant variant_;
};

std::ostream& operator<<(std::ostream& os,
                         IsolatedWebAppStorageLocation location);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_STORAGE_LOCATION_H_
