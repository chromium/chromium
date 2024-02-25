// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/origin_keyed_permission_action_service.h"

#include <utility>
#include "components/permissions/request_type.h"

namespace permissions {

OriginKeyedPermissionActionService::OriginKeyedPermissionActionService() =
    default;
OriginKeyedPermissionActionService::~OriginKeyedPermissionActionService() =
    default;

std::optional<permissions::PermissionActionTime>
OriginKeyedPermissionActionService::GetLastActionEntry(
    const GURL& origin,
    ContentSettingsType type) {
  auto origin_it = map_.find(origin);
  if (origin_it != map_.end()) {
    auto content_settings_it = origin_it->second.find(type);
    if (content_settings_it != origin_it->second.end()) {
      return content_settings_it->second;
    }
  }
  return std::nullopt;
}

void OriginKeyedPermissionActionService::RecordAction(
    const GURL& origin,
    ContentSettingsType type,
    permissions::PermissionAction action) {
  map_[origin][type] = std::make_pair(action, base::TimeTicks::Now());
}

void OriginKeyedPermissionActionService::RecordActionWithTimeForTesting(
    const GURL& origin,
    ContentSettingsType type,
    permissions::PermissionAction action,
    base::TimeTicks time) {
  map_[origin][type] = std::make_pair(action, time);
}

}  // namespace permissions
