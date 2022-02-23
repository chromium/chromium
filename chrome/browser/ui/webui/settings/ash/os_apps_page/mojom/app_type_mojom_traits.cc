// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_type_mojom_traits.h"

#include <utility>

namespace mojo {

Readiness EnumTraits<Readiness, apps::Readiness>::ToMojom(
    apps::Readiness input) {
  switch (input) {
    case apps::Readiness::kUnknown:
      return Readiness::kUnknown;
    case apps::Readiness::kReady:
      return Readiness::kReady;
    case apps::Readiness::kDisabledByBlocklist:
      return Readiness::kDisabledByBlocklist;
    case apps::Readiness::kDisabledByPolicy:
      return Readiness::kDisabledByPolicy;
    case apps::Readiness::kDisabledByUser:
      return Readiness::kDisabledByUser;
    case apps::Readiness::kTerminated:
      return Readiness::kTerminated;
    case apps::Readiness::kUninstalledByUser:
      return Readiness::kUninstalledByUser;
    case apps::Readiness::kRemoved:
      return Readiness::kRemoved;
    case apps::Readiness::kUninstalledByMigration:
      return Readiness::kUninstalledByMigration;
  }
}

bool EnumTraits<Readiness, apps::Readiness>::FromMojom(
    Readiness input,
    apps::Readiness* output) {
  switch (input) {
    case Readiness::kUnknown:
      *output = apps::Readiness::kUnknown;
      return true;
    case Readiness::kReady:
      *output = apps::Readiness::kReady;
      return true;
    case Readiness::kDisabledByBlocklist:
      *output = apps::Readiness::kDisabledByBlocklist;
      return true;
    case Readiness::kDisabledByPolicy:
      *output = apps::Readiness::kDisabledByPolicy;
      return true;
    case Readiness::kDisabledByUser:
      *output = apps::Readiness::kDisabledByUser;
      return true;
    case Readiness::kTerminated:
      *output = apps::Readiness::kTerminated;
      return true;
    case Readiness::kUninstalledByUser:
      *output = apps::Readiness::kUninstalledByUser;
      return true;
    case Readiness::kRemoved:
      *output = apps::Readiness::kRemoved;
      return true;
    case Readiness::kUninstalledByMigration:
      *output = apps::Readiness::kUninstalledByMigration;
      return true;
  }
}

bool StructTraits<PermissionDataView, apps::PermissionPtr>::Read(
    PermissionDataView data,
    apps::PermissionPtr* out) {
  apps::PermissionType permission_type;
  if (!data.ReadPermissionType(&permission_type))
    return false;

  apps::PermissionValuePtr value;
  if (!data.ReadValue(&value))
    return false;

  *out = std::make_unique<apps::Permission>(permission_type, std::move(value),
                                            data.is_managed());
  return true;
}

PermissionType EnumTraits<PermissionType, apps::PermissionType>::ToMojom(
    apps::PermissionType input) {
  switch (input) {
    case apps::PermissionType::kUnknown:
      return PermissionType::kUnknown;
    case apps::PermissionType::kCamera:
      return PermissionType::kCamera;
    case apps::PermissionType::kLocation:
      return PermissionType::kLocation;
    case apps::PermissionType::kMicrophone:
      return PermissionType::kMicrophone;
    case apps::PermissionType::kNotifications:
      return PermissionType::kNotifications;
    case apps::PermissionType::kContacts:
      return PermissionType::kContacts;
    case apps::PermissionType::kStorage:
      return PermissionType::kStorage;
    case apps::PermissionType::kPrinting:
      return PermissionType::kPrinting;
  }
}

bool EnumTraits<PermissionType, apps::PermissionType>::FromMojom(
    PermissionType input,
    apps::PermissionType* output) {
  switch (input) {
    case PermissionType::kUnknown:
      *output = apps::PermissionType::kUnknown;
      return true;
    case PermissionType::kCamera:
      *output = apps::PermissionType::kCamera;
      return true;
    case PermissionType::kLocation:
      *output = apps::PermissionType::kLocation;
      return true;
    case PermissionType::kMicrophone:
      *output = apps::PermissionType::kMicrophone;
      return true;
    case PermissionType::kNotifications:
      *output = apps::PermissionType::kNotifications;
      return true;
    case PermissionType::kContacts:
      *output = apps::PermissionType::kContacts;
      return true;
    case PermissionType::kStorage:
      *output = apps::PermissionType::kStorage;
      return true;
    case PermissionType::kPrinting:
      *output = apps::PermissionType::kPrinting;
      return true;
  }
}

TriState EnumTraits<TriState, apps::TriState>::ToMojom(apps::TriState input) {
  switch (input) {
    case apps::TriState::kAllow:
      return TriState::kAllow;
    case apps::TriState::kBlock:
      return TriState::kBlock;
    case apps::TriState::kAsk:
      return TriState::kAsk;
  }
}

bool EnumTraits<TriState, apps::TriState>::FromMojom(TriState input,
                                                     apps::TriState* output) {
  switch (input) {
    case TriState::kAllow:
      *output = apps::TriState::kAllow;
      return true;
    case TriState::kBlock:
      *output = apps::TriState::kBlock;
      return true;
    case TriState::kAsk:
      *output = apps::TriState::kAsk;
      return true;
  }
}

PermissionValueDataView::Tag
UnionTraits<PermissionValueDataView, apps::PermissionValuePtr>::GetTag(
    const apps::PermissionValuePtr& r) {
  if (r->bool_value.has_value()) {
    return PermissionValueDataView::Tag::BOOL_VALUE;
  } else if (r->tristate_value.has_value()) {
    return PermissionValueDataView::Tag::TRISTATE_VALUE;
  }
  NOTREACHED();
  return PermissionValueDataView::Tag::BOOL_VALUE;
}

bool UnionTraits<PermissionValueDataView, apps::PermissionValuePtr>::Read(
    PermissionValueDataView data,
    apps::PermissionValuePtr* out) {
  switch (data.tag()) {
    case PermissionValueDataView::Tag::BOOL_VALUE: {
      *out = std::make_unique<apps::PermissionValue>(data.bool_value());
      return true;
    }
    case PermissionValueDataView::Tag::TRISTATE_VALUE: {
      apps::TriState tristate_value;
      if (!data.ReadTristateValue(&tristate_value))
        return false;
      *out = std::make_unique<apps::PermissionValue>(tristate_value);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
