// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/chrome_apps_api_permissions.h"

#include <memory>

#include "chrome/common/apps/platform_apps/media_galleries_permission.h"

using extensions::mojom::APIPermissionID;

namespace chrome_apps_api_permissions {
namespace {

template <typename T>
std::unique_ptr<extensions::APIPermission> CreateAPIPermission(
    const extensions::APIPermissionInfo* permission) {
  return std::make_unique<T>(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr extensions::APIPermissionInfo::InitInfo permissions_to_register[] = {
    {APIPermissionID::kArcAppsPrivate, "arcAppsPrivate"},
    {APIPermissionID::kBrowser, "browser",
     extensions::APIPermissionInfo::
         kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFirstRunPrivate, "firstRunPrivate",
     extensions::APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kMediaGalleries, "mediaGalleries",
     extensions::APIPermissionInfo::kFlagNone,
     &CreateAPIPermission<chrome_apps::MediaGalleriesPermission>},
    {APIPermissionID::kPointerLock, "pointerLock",
     extensions::APIPermissionInfo::
         kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kEnterpriseRemoteApps, "enterprise.remoteApps"},
    {APIPermissionID::kSyncFileSystem, "syncFileSystem"},
};

}  // namespace

base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

}  // namespace chrome_apps_api_permissions
