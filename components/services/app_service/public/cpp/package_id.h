// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// An identifier for an installable app package. Package IDs are stable and
// globally unique across app platforms, which makes them suitable for
// identifying apps when communicating with external systems.
//
// Package ID is a composite key of "App Type" and a string
// "Identifier", which is the platform-specific unique ID for the package.
// Package IDs have a canonical string format of the form
// "<app_type_name>:<identifier>".
//
// Package IDs only support a subset of app types supported by App Service.
// Currently, the supported types are:
//
// AppType | Type name | Identifier value      | Example Package ID string
// --------|-----------|-----------------------|--------------------------
// kArc    | "android" | package name          | "android:com.foo.bar"
// kWeb    | "web"     | processed manifest ID | "web:https://app.com/id"
class COMPONENT_EXPORT(APP_TYPES) PackageId {
 public:
  // Creates a Package ID from App Type and opaque package identifier.
  // `app_type` must be a supported type (Web or ARC), and `identifier` must be
  // a non-empty string.
  PackageId(AppType app_type, base::StringPiece identifier);

  PackageId(const PackageId&);
  PackageId& operator=(const PackageId&);
  bool operator<(const PackageId&) const;
  bool operator==(const PackageId&) const;
  bool operator!=(const PackageId&) const;

  // Parses a package ID from the canonical string format. Returns
  // absl::nullopt if parsing failed.
  static absl::optional<PackageId> FromString(
      base::StringPiece package_id_string);

  // Returns the package ID formatted in canonical string form.
  std::string ToString() const;

  // Returns the app type for the package.
  AppType app_type() const { return app_type_; }

  // Returns the platform-specific identifier for the package (i.e. manifest ID
  // for web apps, package name for ARC apps). The identifier is guaranteed to
  // be non-empty, but no other verification is performed.
  std::string identifier() const { return identifier_; }

 private:
  AppType app_type_;
  std::string identifier_;
};

COMPONENT_EXPORT(APP_TYPES)
std::ostream& operator<<(std::ostream& out, const PackageId& package_id);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PACKAGE_ID_H_
