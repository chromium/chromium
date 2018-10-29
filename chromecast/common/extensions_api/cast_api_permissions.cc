// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/extensions_api/cast_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"

namespace cast_api_permissions {
namespace {

using extensions::APIPermission;
using extensions::APIPermissionInfo;

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// CastPermissionMessageProvider::GetPermissionMessages as well.
APIPermissionInfo::InitInfo permissions_to_register[] = {
    // Register permissions for all extension types.
    {APIPermission::kIdentity, "identity"},
    {APIPermission::kExperimental, "experimental",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kNotifications, "notifications"},

    // Register extension permissions.
    {APIPermission::kAccessibilityFeaturesModify,
     "accessibilityFeatures.modify"},
    {APIPermission::kAccessibilityFeaturesRead, "accessibilityFeatures.read"},
    {APIPermission::kAccessibilityPrivate, "accessibilityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kBookmark, "bookmarks"},
    {APIPermission::kBrailleDisplayPrivate, "brailleDisplayPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kHistory, "history"},
    {APIPermission::kTab, "tabs"},
    {APIPermission::kTts, "tts", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kTtsEngine, "ttsEngine",
     APIPermissionInfo::kFlagCannotBeOptional},

    // Register private permissions.
    {APIPermission::kCommandsAccessibility, "commands.accessibility",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCommandLinePrivate, "commandLinePrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kIdentityPrivate, "identityPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kVirtualKeyboardPrivate, "virtualKeyboardPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

std::vector<extensions::Alias> GetPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  return {extensions::Alias("windows", "tabs")};
}

}  // namespace cast_api_permissions
