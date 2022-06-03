// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_

#include "ash/public/cpp/desk_template.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "ui/base/window_open_disposition.h"

namespace ash {
class DeskTemplate;
}

namespace apps {
class AppRegistryCache;
}

namespace desks_storage {

// DeskTemplateConversion contains helper functions for converting between
// the various representations of desk template storage. These include
// ChromeSync format, DeskTemplate objects, and JSON representation.

namespace desk_template_conversion {

using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using SyncLaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;

// Converts the TabGroupColorId passed into its string equivalent
// as defined in the k constants above.
std::string ConvertTabGroupColorIdToString(tab_groups::TabGroupColorId color);

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time);

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t);

// Converts a JSON desk template to an ash desk template. The returned desk
// template will have source set to `source`.
std::unique_ptr<ash::DeskTemplate> ParseDeskTemplateFromSource(
    const base::Value& policy_json,
    ash::DeskTemplateSource source);

base::Value SerializeDeskTemplateAsPolicy(
    const ash::DeskTemplate* desk_template,
    apps::AppRegistryCache* app_cache);

// Convert sync proto WindowOpenDisposition to base's WindowOpenDisposition.
// This value is cast to int32_t by the caller to be assigned to the
// `disposition` field in AppRestoreData.
WindowOpenDisposition ToBaseWindowOpenDisposition(
    SyncWindowOpenDisposition disposition);

// Convert sync proto WindowOpenDisposition to base's WindowOpenDisposition.
// This value is cast to int32_t by the caller to be assigned to the
// `disposition` field in AppRestoreData.
SyncWindowOpenDisposition FromBaseWindowOpenDisposition(
    WindowOpenDisposition disposition);

// Convert from apps::mojom::LaunchContainer to sunc proto LaunchContainer.
// Assumes caller has cast `container` from int32_t to
// apps::mojom::LaunchContainer.
SyncLaunchContainer FromMojomLaunchContainer(
    apps::mojom::LaunchContainer container);

// Convert sync proto LaunchContainer to apps::Mojom::LaunchContainer
// used in the AppRestoreData `container` field.  This value is cast to
// int32_t by the caller to be assigned to the field in AppRestoreData.
apps::mojom::LaunchContainer ToMojomLaunchContainer(
    SyncLaunchContainer container);

}  // namespace desk_template_conversion

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_
