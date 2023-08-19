// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_

#include "ash/public/cpp/desk_template.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "ui/base/window_open_disposition.h"

namespace apps {
class AppRegistryCache;
}

namespace desks_storage::desk_template_conversion {

// Error codes for parsing a saved desk.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SavedDeskParseError {
  kOk = 0,
  kBaseValueIsNotDict = 1,
  kMissingRequiredFields = 2,
  kInvalidUuid = 3,
  kInvalidDeskType = 4,
  kFileNotExist = 5,
  kInvalidJson = 6,
  kMaxValue = kInvalidJson,
};

using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using SyncLaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;
using ParseSavedDeskResult =
    base::expected<std::unique_ptr<ash::DeskTemplate>, SavedDeskParseError>;

// Converts the TabGroupColorId passed into its string equivalent
// as defined in the k constants above.
std::string ConvertTabGroupColorIdToString(tab_groups::TabGroupColorId color);

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time);

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t);

// Creates a default template from policy.  Template will be created without
// window information.  Expects a list value containing the different template
// definitions.  Schema located at:
// `components/policy/resources/templates/policy_definitions/miscellaneous/...
// ...AppLaunchAutomation.yaml`
std::vector<std::unique_ptr<ash::DeskTemplate>>
ParseAdminTemplatesFromPolicyValue(const base::Value& value);

// Converts a JSON desk template to an ash desk template. The returned desk
// template will have source set to `source`. The policy associated is
// PreconfiguredDeskTemplates.
ParseSavedDeskResult ParseDeskTemplateFromBaseValue(
    const base::Value& value,
    ash::DeskTemplateSource source);

base::Value SerializeDeskTemplateAsBaseValue(
    const ash::DeskTemplate* desk_template,
    apps::AppRegistryCache* app_cache);

// Converts a WorkspaceDesk proto to its corresponding ash::DeskTemplate
std::unique_ptr<ash::DeskTemplate> FromSyncProto(
    const sync_pb::WorkspaceDeskSpecifics& pb_entry);

// Converts an ash::DeskTemplate to its corresponding WorkspaceDesk proto.
sync_pb::WorkspaceDeskSpecifics ToSyncProto(
    const ash::DeskTemplate* desk_template,
    apps::AppRegistryCache* app_cache);
}  // namespace desks_storage::desk_template_conversion

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_
