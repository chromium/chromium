// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace permissions {

PermissionRequest::PermissionRequest() {}

bool PermissionRequest::IsDuplicateOf(PermissionRequest* other_request) const {
  return GetRequestType() == other_request->GetRequestType() &&
         GetOrigin() == other_request->GetOrigin();
}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionRequestGestureType::UNKNOWN;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  return ContentSettingsType::DEFAULT;
}

#if !defined(OS_ANDROID)
absl::optional<std::u16string> PermissionRequest::GetChipText() const {
  return absl::nullopt;
}
#endif

}  // namespace permissions
