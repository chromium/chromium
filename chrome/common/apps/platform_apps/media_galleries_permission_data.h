// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_DATA_H_
#define CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_DATA_H_

#include <memory>
#include <string>

#include "extensions/common/permissions/api_permission.h"

namespace base {
class Value;
}

namespace chrome_apps {

// A MediaGalleriesPermissionData instance represents a single part of the
// MediaGalleriesPermission. e.g. "read" or "allAutoDetected".
class MediaGalleriesPermissionData {
 public:
  MediaGalleriesPermissionData();

  // Check if |param| (which must be a MediaGalleriesPermission::CheckParam)
  // matches the encapsulated attribute.
  bool Check(const extensions::APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  std::unique_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  bool operator<(const MediaGalleriesPermissionData& rhs) const;
  bool operator==(const MediaGalleriesPermissionData& rhs) const;

  std::string permission() const { return permission_; }

  // This accessor is provided for IPC_STRUCT_TRAITS_MEMBER.  Please think
  // twice before using it for anything else.
  std::string& permission() { return permission_; }

 private:
  std::string permission_;
};

}  // namespace chrome_apps

#endif  // CHROME_COMMON_APPS_PLATFORM_APPS_MEDIA_GALLERIES_PERMISSION_DATA_H_
