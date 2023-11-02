// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PERMISSIONS_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PERMISSIONS_H_

#include "base/containers/span.h"
#include "extensions/common/permissions/api_permission.h"

namespace chromeos {
namespace extensions_api_permissions {

// Returns the information necessary to construct the APIPermissions usable in
// chromeos system extensions.
base::span<const extensions::APIPermissionInfo::InitInfo> GetPermissionInfos();

}  // namespace extensions_api_permissions
}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_API_PERMISSIONS_H_
