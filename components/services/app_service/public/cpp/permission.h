// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace apps {

// The types of permissions in App Service.
ENUM(PermissionType,
     kUnknown,
     kCamera,
     kLocation,
     kMicrophone,
     kNotifications,
     kContacts,
     kStorage,
     kPrinting,
     kFileHandling)

ENUM(TriState,
     // Access to the permission is allowed.
     kAllow,
     // Access to the permission is blocked.
     kBlock,
     // The app can ask the user whether to allow the permission. Some
     // Publishers may allow temporary permission access while in the kAsk state
     // (e.g. "Only this time" for ARC apps).
     kAsk)

struct COMPONENT_EXPORT(APP_TYPES) Permission {
  // The value of a permission can be a TriState or a bool, depending on how the
  // publisher represents permissions.
  using PermissionValue = absl::variant<bool, TriState>;

  Permission(PermissionType permission_type,
             PermissionValue value,
             bool is_managed,
             std::optional<std::string> details = std::nullopt);
  Permission(const Permission&) = delete;
  Permission& operator=(const Permission&) = delete;
  ~Permission();

  bool operator==(const Permission& other) const;
  bool operator!=(const Permission& other) const;

  std::unique_ptr<Permission> Clone() const;

  // Checks whether this is equal to permission enabled. If it is TriState, only
  // Allow represent permission enabled.
  bool IsPermissionEnabled() const;

  std::string ToString() const;

  PermissionType permission_type;
  PermissionValue value;
  // If the permission is managed by an enterprise policy.
  bool is_managed;
  // Human-readable string to provide more detail about the state of a
  // permission. May be displayed next to the permission value in UI surfaces.
  // e.g. a kLocation permission might have `details` of "While in use" or
  // "Approximate location only".
  std::optional<std::string> details;
};

using PermissionPtr = std::unique_ptr<Permission>;
using Permissions = std::vector<PermissionPtr>;

// Creates a deep copy of `source_permissions`.
COMPONENT_EXPORT(APP_TYPES)
Permissions ClonePermissions(const Permissions& source_permissions);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const Permissions& source, const Permissions& target);

// Converts `permission` to base::Value::Dict, e.g.:
// {
//   "PermissionType": 3,
//   "TriState": 2,
//   "is_managed": false,
//   "details": "xyz",
// }
COMPONENT_EXPORT(APP_TYPES)
base::Value::Dict ConvertPermissionToDict(const PermissionPtr& permission);

// Converts base::Value::Dict to PermissionPtr.
COMPONENT_EXPORT(APP_TYPES)
PermissionPtr ConvertDictToPermission(const base::Value::Dict& dict);

// Converts `permissions` to base::Value::List, e.g.:
// {
//   {
//     "PermissionType": 3,
//     "TriState": 2,
//     "is_managed": false,
//     "details": "xyz",
//   },
//   {
//     "PermissionType": 1,
//     "Value": true,
//     "is_managed": true,
//   },
// }
COMPONENT_EXPORT(APP_TYPES)
base::Value::List ConvertPermissionsToList(const Permissions& permissions);

// Converts base::Value::List to Permissions.
COMPONENT_EXPORT(APP_TYPES)
Permissions ConvertListToPermissions(const base::Value::List* list);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_
