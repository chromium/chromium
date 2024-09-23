// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

// An identifier for an installable app package. Package IDs are stable and
// globally unique across app platforms, which makes them suitable for
// identifying apps when communicating with external systems.
//
// Package ID is a composite key of "Package Type" and a string
// "Identifier", which is the platform-specific unique ID for the package.
// Package IDs have a canonical string format of the form
// "<package_type_name>:<identifier>".
//
// Package IDs only support a subset of app types supported by App Service.
// Currently, the supported types are:
//
// PackageType | Type name   | Identifier value      | Example Package ID string
// ------------|-------------|-----------------------|--------------------------
// kArc        | "android"   | package name          | "android:com.foo.bar"
// kBorealis   | "steam"     | Steam Game ID         | "steam:123456"
// kChromeApp  | "chromeapp" | Extension ID          | "chromeapp:mmfbcljfglbok"
// kGeForceNow | "gfn"       | GeForce Game ID       | "gfn:123456"
// kSystem     | "system"    | policy ID             | "system:file_manager"
// kWeb        | "web"       | processed manifest ID | "web:https://app.com/id"
// kWebsite    | "website"   | start URL             | "website:https://app.co/"
class COMPONENT_EXPORT(APP_TYPES) PackageId {
 public:
  // Creates a Package ID from PackageType and opaque package identifier.
  // `package_type` must be a supported type from the table above, and
  // `identifier` must be a non-empty string.
  PackageId(PackageType package_type, std::string_view identifier);
  // Creates a PackageId for an unknown package.
  PackageId();

  PackageId(const PackageId&);
  PackageId& operator=(const PackageId&);
  bool operator<(const PackageId&) const;
  bool operator==(const PackageId&) const;
  bool operator!=(const PackageId&) const;

  // Parses a package ID from the canonical string format. Returns
  // std::nullopt if parsing failed. This method will never parse an "unknown"
  // PackageId. That is, `PackageId::FromString(PackageId().ToString())` returns
  // std::nullopt.
  static std::optional<PackageId> FromString(
      std::string_view package_id_string);

  // Returns the package ID formatted in canonical string form.
  std::string ToString() const;

  // Returns the package type for the package. The type can be
  // PackageType::kUnknown if the PackageId is for an unknown package.
  PackageType package_type() const { return package_type_; }

  // Returns the platform-specific identifier for the package (i.e. manifest ID
  // for web apps, package name for ARC apps). The identifier is guaranteed to
  // be non-empty, but no other verification is performed.
  const std::string& identifier() const { return identifier_; }

 private:
  PackageType package_type_;
  std::string identifier_;
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& out, const PackageId& package_id);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_
