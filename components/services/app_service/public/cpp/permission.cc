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

PermissionValue::PermissionValue(bool bool_value) : value(bool_value) {}

PermissionValue::PermissionValue(TriState tristate_value)
    : value(tristate_value) {}

PermissionValue::~PermissionValue() = default;

bool PermissionValue::operator==(const PermissionValue& other) const {
  if (absl::holds_alternative<bool>(value) &&
      absl::holds_alternative<bool>(other.value)) {
    return absl::get<bool>(value) == absl::get<bool>(other.value);
  }
  if (absl::holds_alternative<TriState>(value) &&
      absl::holds_alternative<TriState>(other.value)) {
    return absl::get<TriState>(value) == absl::get<TriState>(other.value);
  }
  return false;
}

std::unique_ptr<PermissionValue> PermissionValue::Clone() const {
  if (absl::holds_alternative<bool>(value)) {
    return std::make_unique<PermissionValue>(absl::get<bool>(value));
  }
  if (absl::holds_alternative<TriState>(value)) {
    return std::make_unique<PermissionValue>(absl::get<TriState>(value));
  }
  return nullptr;
}

bool PermissionValue::IsPermissionEnabled() const {
  if (absl::holds_alternative<bool>(value)) {
    return absl::get<bool>(value);
  }
  if (absl::holds_alternative<TriState>(value)) {
    return absl::get<TriState>(value) == TriState::kAllow;
  }
  return false;
}

Permission::Permission(PermissionType permission_type,
                       PermissionValuePtr value,
                       bool is_managed)
    : permission_type(permission_type),
      value(std::move(value)),
      is_managed(is_managed) {}

Permission::~Permission() = default;

bool Permission::operator==(const Permission& other) const {
  return permission_type == other.permission_type &&
         ((!value && !other.value) || (*value == *other.value)) &&
         is_managed == other.is_managed;
}

bool Permission::operator!=(const Permission& other) const {
  return !(*this == other);
}

PermissionPtr Permission::Clone() const {
  if (!value) {
    return nullptr;
  }

  return std::make_unique<Permission>(permission_type, value->Clone(),
                                      is_managed);
}

bool Permission::IsPermissionEnabled() const {
  return value && value->IsPermissionEnabled();
}

std::string Permission::ToString() const {
  std::stringstream out;
  out << " permission type: " << EnumToString(permission_type);
  out << " value: " << std::endl;
  if (value) {
    if (absl::holds_alternative<bool>(value->value)) {
      out << " bool_value: "
          << (absl::get<bool>(value->value) ? "true" : "false");
    } else if (absl::holds_alternative<TriState>(value->value)) {
      out << " tristate_value: "
          << EnumToString(absl::get<TriState>(value->value));
    }
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
