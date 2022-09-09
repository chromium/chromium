// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/apps/platform_apps/media_galleries_permission_data.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"

namespace chrome_apps {

MediaGalleriesPermissionData::MediaGalleriesPermissionData() {}

bool MediaGalleriesPermissionData::Check(
    const extensions::APIPermission::CheckParam* param) const {
  if (!param)
    return false;

  const MediaGalleriesPermission::CheckParam& specific_param =
      *static_cast<const MediaGalleriesPermission::CheckParam*>(param);
  return permission_ == specific_param.permission;
}

std::unique_ptr<base::Value> MediaGalleriesPermissionData::ToValue() const {
  return std::make_unique<base::Value>(permission_);
}

bool MediaGalleriesPermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const std::string* raw_permission = value->GetIfString();
  if (!raw_permission)
    return false;

  std::string permission;
  base::TrimWhitespaceASCII(*raw_permission, base::TRIM_ALL, &permission);

  if (permission == MediaGalleriesPermission::kAllAutoDetectedPermission ||
      permission == MediaGalleriesPermission::kReadPermission ||
      permission == MediaGalleriesPermission::kCopyToPermission ||
      permission == MediaGalleriesPermission::kDeletePermission) {
    permission_ = permission;
    return true;
  }
  return false;
}

bool MediaGalleriesPermissionData::operator<(
    const MediaGalleriesPermissionData& rhs) const {
  return permission_ < rhs.permission_;
}

bool MediaGalleriesPermissionData::operator==(
    const MediaGalleriesPermissionData& rhs) const {
  return permission_ == rhs.permission_;
}

}  // namespace chrome_apps
