// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/extensions/chromeos_extensions_api_permissions.h"

#include "base/containers/span.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

namespace ash::extensions_api_permissions {

namespace {

using extensions::APIPermissionInfo;
using extensions::mojom::APIPermissionID;

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo kPermissionsToRegister[] = {
    {APIPermissionID::kSpeechRecognitionPrivate, "speechRecognitionPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return kPermissionsToRegister;
}

}  // namespace ash::extensions_api_permissions
