// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/package_id.h"

#include <optional>
#include <ostream>
#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

namespace {
constexpr char kUnknownName[] = "unknown";
constexpr char kArcPlatformName[] = "android";
constexpr char kBorealisPlatformName[] = "steam";
constexpr char kChromeAppPlatformName[] = "chromeapp";
constexpr char kGeForceNowPlatformName[] = "gfn";
constexpr char kSystemPlatformName[] = "system";
constexpr char kWebPlatformName[] = "web";
constexpr char kWebShortcutPlatformName[] = "website";

PackageType PlatformNameToPackageType(std::string_view platform_name) {
  if (platform_name == kArcPlatformName) {
    return PackageType::kArc;
  }
  if (platform_name == kBorealisPlatformName) {
    return PackageType::kBorealis;
  }
  if (platform_name == kChromeAppPlatformName) {
    return PackageType::kChromeApp;
  }
  if (platform_name == kGeForceNowPlatformName) {
    return PackageType::kGeForceNow;
  }
  if (platform_name == kSystemPlatformName) {
    return PackageType::kSystem;
  }
  if (platform_name == kWebPlatformName) {
    return PackageType::kWeb;
  }
  if (platform_name == kWebShortcutPlatformName) {
    return PackageType::kWebsite;
  }

  return PackageType::kUnknown;
}

std::string_view PackageTypeToPlatformName(PackageType package_type) {
  switch (package_type) {
    case PackageType::kUnknown:
      return kUnknownName;
    case PackageType::kArc:
      return kArcPlatformName;
    case PackageType::kBorealis:
      return kBorealisPlatformName;
    case PackageType::kChromeApp:
      return kChromeAppPlatformName;
    case PackageType::kGeForceNow:
      return kGeForceNowPlatformName;
    case PackageType::kSystem:
      return kSystemPlatformName;
    case PackageType::kWeb:
      return kWebPlatformName;
    case PackageType::kWebsite:
      return kWebShortcutPlatformName;
  }
}

}  // namespace

PackageId::PackageId(PackageType package_type, std::string_view identifier)
    : package_type_(package_type), identifier_(identifier) {
  DCHECK(!identifier_.empty());
}

PackageId::PackageId() : PackageId(PackageType::kUnknown, kUnknownName) {}

PackageId::PackageId(const PackageId&) = default;
PackageId& PackageId::operator=(const PackageId&) = default;

bool PackageId::operator<(const PackageId& rhs) const {
  if (this->package_type_ < rhs.package_type_) {
    return true;
  } else if (this->package_type_ > rhs.package_type_) {
    return false;
  }
  // If we're here, it's because package_type_ == rhs.package_type_.
  if (this->identifier_ < rhs.identifier_) {
    return true;
  } else {
    return false;
  }
}

bool PackageId::operator==(const PackageId& rhs) const {
  return this->package_type_ == rhs.package_type_ &&
         this->identifier_ == rhs.identifier_;
}

bool PackageId::operator!=(const PackageId& rhs) const {
  return this->package_type_ != rhs.package_type_ ||
         this->identifier_ != rhs.identifier_;
}

// static
std::optional<PackageId> PackageId::FromString(
    std::string_view package_id_string) {
  size_t separator = package_id_string.find_first_of(':');
  if (separator == std::string::npos ||
      separator == package_id_string.size() - 1) {
    return std::nullopt;
  }

  PackageType type =
      PlatformNameToPackageType(package_id_string.substr(0, separator));
  if (type == PackageType::kUnknown) {
    return std::nullopt;
  }

  return PackageId(type, package_id_string.substr(separator + 1));
}

std::string PackageId::ToString() const {
  return base::StrCat(
      {PackageTypeToPlatformName(package_type_), ":", identifier_});
}

std::ostream& operator<<(std::ostream& out, const PackageId& package_id) {
  return out << package_id.ToString();
}

}  // namespace apps
