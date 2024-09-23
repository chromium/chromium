// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/permission.h"

#include <sstream>

#include "base/containers/to_value_list.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace apps {

namespace {

const char kPermissionTypeKey[] = "permission_type";
const char kValueKey[] = "value";
const char kIsManagedKey[] = "is_managed";
const char kDetailsKey[] = "details";

}  // namespace

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
                       std::optional<std::string> details)
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

base::Value::Dict ConvertPermissionToDict(const PermissionPtr& permission) {
  base::Value::Dict dict;

  if (!permission) {
    return dict;
  }

  dict.Set(kPermissionTypeKey, static_cast<int>(permission->permission_type));

  if (absl::holds_alternative<bool>(permission->value)) {
    dict.Set(kValueKey, absl::get<bool>(permission->value));
  } else if (absl::holds_alternative<TriState>(permission->value)) {
    dict.Set(kValueKey,
             static_cast<int>(absl::get<TriState>(permission->value)));
  }

  dict.Set(kIsManagedKey, permission->is_managed);

  if (permission->details.has_value()) {
    dict.Set(kDetailsKey, permission->details.value());
  }

  return dict;
}

PermissionPtr ConvertDictToPermission(const base::Value::Dict& dict) {
  std::optional<int> permission_type = dict.FindInt(kPermissionTypeKey);
  if (!permission_type.has_value() ||
      permission_type.value() < static_cast<int>(PermissionType::kUnknown) ||
      permission_type.value() > static_cast<int>(PermissionType::kMaxValue)) {
    return nullptr;
  }

  Permission::PermissionValue permission_value;
  std::optional<bool> value = dict.FindBool(kValueKey);
  if (value.has_value()) {
    permission_value = value.value();
  } else {
    std::optional<int> tri_state = dict.FindInt(kValueKey);
    if (tri_state.has_value() &&
        tri_state.value() >= static_cast<int>(TriState::kAllow) &&
        tri_state.value() <= static_cast<int>(TriState::kMaxValue)) {
      permission_value = static_cast<TriState>(tri_state.value());
    } else {
      return nullptr;
    }
  }

  std::optional<bool> is_managed = dict.FindBool(kIsManagedKey);
  if (!is_managed.has_value()) {
    return nullptr;
  }

  const std::string* details = dict.FindString(kDetailsKey);

  return std::make_unique<Permission>(
      static_cast<PermissionType>(permission_type.value()), permission_value,
      is_managed.value(),
      details ? std::optional<std::string>(*details) : std::nullopt);
}

base::Value::List ConvertPermissionsToList(const Permissions& permissions) {
  return base::ToValueList(permissions, &ConvertPermissionToDict);
}

Permissions ConvertListToPermissions(const base::Value::List* list) {
  Permissions permissions;

  if (!list) {
    return permissions;
  }

  for (const base::Value& permission : *list) {
    PermissionPtr parsed_permission =
        ConvertDictToPermission(permission.GetDict());
    if (parsed_permission) {
      permissions.push_back(std::move(parsed_permission));
    }
  }
  return permissions;
}

}  // namespace apps
