// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_H_
#define CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_H_

#include "chrome/common/apps/platform_apps/chrome_apps_messages.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission_data.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/set_disjunction_permission.h"

namespace chrome_apps {

// Media Galleries permissions are as follows:
//   <media-galleries-permission-pattern>
//             := <access> | <access> 'allAutoDetected' | 'allAutoDetected' |
//                <access> 'scan' | 'scan'
//   <access>  := 'read' | 'read' <access> | 'read' <secondary-access>
//   <secondary-access>
//             := 'delete' | 'delete' <secondary-access> |
//                'delete' <tertiary-access>
//   <tertiary-access>
//             := 'copyTo' | 'copyTo' <tertiary-access>
// An example of a line for mediaGalleries permissions in a manifest file:
//   {"mediaGalleries": "read delete"},
// We also allow a permission without any sub-permissions:
//   "mediaGalleries",
// TODO(devlin): Move this class to chrome/common/apps/platform_apps.
class MediaGalleriesPermission
    : public extensions::SetDisjunctionPermission<MediaGalleriesPermissionData,
                                                  MediaGalleriesPermission> {
 public:
  struct CheckParam : public extensions::APIPermission::CheckParam {
    explicit CheckParam(const std::string& permission)
        : permission(permission) {}
    const std::string permission;
  };

  explicit MediaGalleriesPermission(const extensions::APIPermissionInfo* info);
  ~MediaGalleriesPermission() override;

  // SetDisjunctionPermission overrides.
  // MediaGalleriesPermission does additional checks to make sure the
  // permissions do not contain unknown values.
  bool FromValue(const base::Value* value,
                 std::string* error,
                 std::vector<std::string>* unhandled_permissions) override;

  // extensions::APIPermission overrides.
  extensions::PermissionIDSet GetPermissions() const override;

  // Permission strings.
  static const char kAllAutoDetectedPermission[];
  static const char kScanPermission[];
  static const char kReadPermission[];
  static const char kCopyToPermission[];
  static const char kDeletePermission[];
};

}  // namespace chrome_apps

#endif  // CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_H_
