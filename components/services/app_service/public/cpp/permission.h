// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_

#include <utility>
#include <vector>

#include "base/component_export.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

ENUM(TriState, kAllow, kBlock, kAsk)

// The permission value could be a TriState or a bool
struct COMPONENT_EXPORT(APP_TYPES) PermissionValue {
  explicit PermissionValue(bool bool_value);
  explicit PermissionValue(TriState tristate_value);
  PermissionValue(const PermissionValue&) = delete;
  PermissionValue& operator=(const PermissionValue&) = delete;
  ~PermissionValue();

  bool operator==(const PermissionValue& other) const;

  std::unique_ptr<PermissionValue> Clone() const;

  // Checks whether this is equal to permission enabled. If it is TriState, only
  // Allow represent permission enabled.
  bool IsPermissionEnabled() const;

  absl::variant<bool, TriState> value;
};

using PermissionValuePtr = std::unique_ptr<PermissionValue>;

struct COMPONENT_EXPORT(APP_TYPES) Permission {
  Permission(PermissionType permission_type,
             PermissionValuePtr value,
             bool is_managed);
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
  std::unique_ptr<PermissionValue> value;
  // If the permission is managed by an enterprise policy.
  bool is_managed;
};

using PermissionPtr = std::unique_ptr<Permission>;
using Permissions = std::vector<PermissionPtr>;

// Creates a deep copy of `source_permissions`.
COMPONENT_EXPORT(APP_TYPES)
Permissions ClonePermissions(const Permissions& source_permissions);

COMPONENT_EXPORT(APP_TYPES)
bool IsEqual(const Permissions& source, const Permissions& target);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PERMISSION_H_
