// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/permission_resolver.h"

#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"

namespace permissions {

PermissionResolver::PermissionResolver(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type),
      request_type_(
          ContentSettingsTypeToRequestTypeIfExists(content_settings_type)) {}

PermissionResolver::PermissionResolver(RequestType request_type)
    : content_settings_type_(RequestTypeToContentSettingsType(request_type)),
      request_type_(request_type) {}

PermissionResolver::PromptParameters::PromptParameters() = default;
PermissionResolver::PromptParameters::~PromptParameters() = default;

}  // namespace permissions
