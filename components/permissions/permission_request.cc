// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace permissions {

PermissionRequest::PermissionRequest() {}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionRequestGestureType::UNKNOWN;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  return ContentSettingsType::DEFAULT;
}

#if !defined(OS_ANDROID)
base::Optional<std::u16string> PermissionRequest::GetChipText() const {
  return base::nullopt;
}
#endif

#if defined(OS_ANDROID)
std::u16string PermissionRequest::GetQuietTitleText() const {
  return std::u16string();
}

std::u16string PermissionRequest::GetQuietMessageText() const {
  return GetMessageText();
}
#endif

}  // namespace permissions
