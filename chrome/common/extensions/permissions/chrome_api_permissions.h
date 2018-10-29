// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_API_PERMISSIONS_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_API_PERMISSIONS_H_

#include "base/containers/span.h"
#include "extensions/common/alias.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {
namespace chrome_api_permissions {

// Returns the information necessary to construct chrome-layer extension
// APIPermissions.
base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos();

// Returns the list of aliases for chrome-layer extension APIPermissions.
base::span<const Alias> GetPermissionAliases();

}  // namespace chrome_api_permissions
}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_API_PERMISSIONS_H_
