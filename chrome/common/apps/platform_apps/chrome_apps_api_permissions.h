// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_
#define CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_

#include "base/containers/span.h"
#include "extensions/common/permissions/api_permission.h"

namespace chrome_apps_api_permissions {

// Returns the information necessary to construct Chrome app-specific
// APIPermissions.
base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos();

}  // namespace chrome_apps_api_permissions

#endif  // CHROME_COMMON_APPS_PLATFORM_APPS_CHROME_APPS_API_PERMISSIONS_H_
