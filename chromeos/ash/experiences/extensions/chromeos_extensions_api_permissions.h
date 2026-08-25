// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_EXTENSIONS_CHROMEOS_EXTENSIONS_API_PERMISSIONS_H_
#define CHROMEOS_ASH_EXPERIENCES_EXTENSIONS_CHROMEOS_EXTENSIONS_API_PERMISSIONS_H_

#include "base/containers/span.h"
#include "extensions/common/permissions/api_permission.h"

namespace ash::extensions_api_permissions {

// Returns the information necessary to construct the APIPermissions usable in
// chromeos system extensions.
base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos();

}  // namespace ash::extensions_api_permissions

#endif  // CHROMEOS_ASH_EXPERIENCES_EXTENSIONS_CHROMEOS_EXTENSIONS_API_PERMISSIONS_H_
