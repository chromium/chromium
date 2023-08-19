// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/permission.h"

#include <sstream>

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace apps {

APP_ENUM_TO_STRING(PermissionType,
                   kUnknown,
                   kCamera,
                   kLocation,
                   kMicrophone,
                   kNotifications,
                   kContacts,
                   kStorage,
                   kPrinting,
                   kFileHandling)
APP_ENUM_TO_STRING(TriState, kAllow, kBlock, kAsk)

Permission::Permission(PermissionType permission_type,
                       PermissionValue value,
                       bool is_managed,
                       absl::optional<std::string> details)
    : permission_type(permission_type),
      value(std::move(value)),
      is_managed(is_managed),
      details(std::move(details)) {}

Permission::~Permission() = default;

bool Permission::operator==(const Permission& other) const {
  return permission_type == other.permission_type && value == other.value &&
         is_managed == other.is_managed && details == other.details;
}

bool Permission::operator!=(const Permission& other) const {
  return !(*this == other);
}

PermissionPtr Permission::Clone() const {
  return std::make_unique<Permission>(permission_type, value, is_managed,
                                      details);
}

bool Permission::IsPermissionEnabled() const {
  if (absl::holds_alternative<bool>(value)) {
    return absl::get<bool>(value);
  }
  if (absl::holds_alternative<TriState>(value)) {
    return absl::get<TriState>(value) == TriState::kAllow;
  }
  return false;
}

std::string Permission::ToString() const {
  std::stringstream out;
  out << " permission type: " << EnumToString(permission_type) << std::endl;
  if (absl::holds_alternative<bool>(value)) {
    out << " bool_value: " << (absl::get<bool>(value) ? "true" : "false");
  } else if (absl::holds_alternative<TriState>(value)) {
    out << " tristate_value: " << EnumToString(absl::get<TriState>(value));
  }
  out << std::endl;
  if (details.has_value()) {
    out << " details: " << details.value() << std::endl;
  }
  out << " is_managed: " << (is_managed ? "true" : "false") << std::endl;
  return out.str();
}

Permissions ClonePermissions(const Permissions& source_permissions) {
  Permissions permissions;
  for (const auto& permission : source_permissions) {
    permissions.push_back(permission->Clone());
  }
  return permissions;
}

bool IsEqual(const Permissions& source, const Permissions& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace apps
