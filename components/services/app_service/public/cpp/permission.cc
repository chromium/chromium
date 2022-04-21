// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/permission.h"

namespace apps {

APP_ENUM_TO_STRING(PermissionType,
                   kUnknown,
                   kCamera,
                   kLocation,
                   kMicrophone,
                   kNotifications,
                   kContacts,
                   kStorage,
                   kPrinting)
APP_ENUM_TO_STRING(TriState, kAllow, kBlock, kAsk)

PermissionValue::PermissionValue(bool bool_value) : bool_value(bool_value) {}

PermissionValue::PermissionValue(TriState tristate_value)
    : tristate_value(tristate_value) {}

PermissionValue::~PermissionValue() = default;

bool PermissionValue::operator==(const PermissionValue& other) const {
  if (tristate_value.has_value() && other.tristate_value.has_value()) {
    return tristate_value.value() == other.tristate_value.value();
  }

  if (bool_value.has_value() && other.bool_value.has_value()) {
    return bool_value.value() == other.bool_value.value();
  }

  return false;
}

std::unique_ptr<PermissionValue> PermissionValue::Clone() const {
  if (tristate_value.has_value()) {
    return std::make_unique<PermissionValue>(tristate_value.value());
  }

  if (bool_value.has_value()) {
    return std::make_unique<PermissionValue>(bool_value.value());
  }

  return nullptr;
}

bool PermissionValue::IsPermissionEnabled() const {
  if (tristate_value.has_value()) {
    return tristate_value.value() == TriState::kAllow;
  } else if (bool_value.has_value()) {
    return bool_value.value();
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
  if (value && value->bool_value.has_value()) {
    out << " bool_value: " << (value->bool_value.value() ? "true" : "false");
  }
  if (value && value->tristate_value.has_value()) {
    out << " tristate_value: " << EnumToString(value->tristate_value.value());
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

PermissionType ConvertMojomPermissionTypeToPermissionType(
    apps::mojom::PermissionType mojom_permission_type) {
  switch (mojom_permission_type) {
    case apps::mojom::PermissionType::kUnknown:
      return PermissionType::kUnknown;
    case apps::mojom::PermissionType::kCamera:
      return PermissionType::kCamera;
    case apps::mojom::PermissionType::kLocation:
      return PermissionType::kLocation;
    case apps::mojom::PermissionType::kMicrophone:
      return PermissionType::kMicrophone;
    case apps::mojom::PermissionType::kNotifications:
      return PermissionType::kNotifications;
    case apps::mojom::PermissionType::kContacts:
      return PermissionType::kContacts;
    case apps::mojom::PermissionType::kStorage:
      return PermissionType::kStorage;
    case apps::mojom::PermissionType::kPrinting:
      return PermissionType::kPrinting;
  }
}

apps::mojom::PermissionType ConvertPermissionTypeToMojomPermissionType(
    PermissionType permission_type) {
  switch (permission_type) {
    case PermissionType::kUnknown:
      return apps::mojom::PermissionType::kUnknown;
    case PermissionType::kCamera:
      return apps::mojom::PermissionType::kCamera;
    case PermissionType::kLocation:
      return apps::mojom::PermissionType::kLocation;
    case PermissionType::kMicrophone:
      return apps::mojom::PermissionType::kMicrophone;
    case PermissionType::kNotifications:
      return apps::mojom::PermissionType::kNotifications;
    case PermissionType::kContacts:
      return apps::mojom::PermissionType::kContacts;
    case PermissionType::kStorage:
      return apps::mojom::PermissionType::kStorage;
    case PermissionType::kPrinting:
      return apps::mojom::PermissionType::kPrinting;
  }
}

TriState ConvertMojomTriStateToTriState(apps::mojom::TriState mojom_tri_state) {
  switch (mojom_tri_state) {
    case apps::mojom::TriState::kAllow:
      return TriState::kAllow;
    case apps::mojom::TriState::kBlock:
      return TriState::kBlock;
    case apps::mojom::TriState::kAsk:
      return TriState::kAsk;
  }
}

apps::mojom::TriState ConvertTriStateToMojomTriState(TriState tri_state) {
  switch (tri_state) {
    case TriState::kAllow:
      return apps::mojom::TriState::kAllow;
    case TriState::kBlock:
      return apps::mojom::TriState::kBlock;
    case TriState::kAsk:
      return apps::mojom::TriState::kAsk;
  }
}

PermissionValuePtr ConvertMojomPermissionValueToPermissionValue(
    const apps::mojom::PermissionValuePtr& mojom_permission_value) {
  if (!mojom_permission_value) {
    return nullptr;
  }

  if (mojom_permission_value->is_tristate_value()) {
    return std::make_unique<PermissionValue>(ConvertMojomTriStateToTriState(
        mojom_permission_value->get_tristate_value()));
  } else if (mojom_permission_value->is_bool_value()) {
    return std::make_unique<PermissionValue>(
        mojom_permission_value->get_bool_value());
  }
  return nullptr;
}

apps::mojom::PermissionValuePtr ConvertPermissionValueToMojomPermissionValue(
    const PermissionValuePtr& permission_value) {
  if (!permission_value) {
    return nullptr;
  }

  if (permission_value->bool_value.has_value()) {
    return apps::mojom::PermissionValue::NewBoolValue(
        permission_value->bool_value.value());
  }
  if (permission_value->tristate_value.has_value()) {
    return apps::mojom::PermissionValue::NewTristateValue(
        ConvertTriStateToMojomTriState(
            permission_value->tristate_value.value()));
  }

  NOTREACHED();
  return nullptr;
}

PermissionPtr ConvertMojomPermissionToPermission(
    const apps::mojom::PermissionPtr& mojom_permission) {
  if (!mojom_permission) {
    return nullptr;
  }

  return std::make_unique<Permission>(
      ConvertMojomPermissionTypeToPermissionType(
          mojom_permission->permission_type),
      ConvertMojomPermissionValueToPermissionValue(mojom_permission->value),
      mojom_permission->is_managed);
}

apps::mojom::PermissionPtr ConvertPermissionToMojomPermission(
    const PermissionPtr& permission) {
  auto mojom_permission = apps::mojom::Permission::New();
  if (!permission) {
    return mojom_permission;
  }

  mojom_permission->permission_type =
      ConvertPermissionTypeToMojomPermissionType(permission->permission_type);
  mojom_permission->value =
      ConvertPermissionValueToMojomPermissionValue(permission->value);
  mojom_permission->is_managed = permission->is_managed;
  return mojom_permission;
}

}  // namespace apps
