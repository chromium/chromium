// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_H_
#define COMPONENTS_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_H_

#include <map>
#include <utility>
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"
#include "url/gurl.h"

namespace permissions {

enum class PermissionAction;
enum class RequestType;

typedef std::pair<PermissionAction, base::TimeTicks> PermissionActionTime;

// Service that keeps track of the last permission action per content setting,
// keyed on origins. The data is kept in-memory only and is not persisted across
// browser sessions. This service is used for metrics evaluation.
class OriginKeyedPermissionActionService : public KeyedService {
 public:
  OriginKeyedPermissionActionService();
  ~OriginKeyedPermissionActionService() override;

  OriginKeyedPermissionActionService(
      const OriginKeyedPermissionActionService&) = delete;
  OriginKeyedPermissionActionService& operator=(
      const OriginKeyedPermissionActionService&) = delete;

  std::optional<PermissionActionTime> GetLastActionEntry(
      const GURL& origin,
      ContentSettingsType type);

  void RecordAction(const GURL& origin,
                    ContentSettingsType type,
                    PermissionAction action);

  void RecordActionWithTimeForTesting(const GURL& origin,
                                      ContentSettingsType type,
                                      PermissionAction action,
                                      base::TimeTicks time);

 private:
  std::map<GURL, std::map<ContentSettingsType, PermissionActionTime>> map_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ORIGIN_KEYED_PERMISSION_ACTION_SERVICE_H_
