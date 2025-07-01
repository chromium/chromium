// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_result.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

RevokedPermissionsResult::RevokedPermissionsResult() = default;
RevokedPermissionsResult::~RevokedPermissionsResult() = default;

RevokedPermissionsResult::RevokedPermissionsResult(
    const RevokedPermissionsResult&) = default;

std::unique_ptr<SafetyHubResult> RevokedPermissionsResult::Clone() const {
  return std::make_unique<RevokedPermissionsResult>(*this);
}

void RevokedPermissionsResult::AddRevokedPermission(
    PermissionsData permissions_data) {
  revoked_permissions_.push_back(std::move(permissions_data));
}

const std::list<PermissionsData>&
RevokedPermissionsResult::GetRevokedPermissions() {
  return revoked_permissions_;
}

std::set<ContentSettingsPattern> RevokedPermissionsResult::GetRevokedOrigins()
    const {
  std::set<ContentSettingsPattern> origins;
  for (const auto& permission : revoked_permissions_) {
    origins.insert(permission.primary_pattern);
  }
  return origins;
}

base::Value::Dict RevokedPermissionsResult::ToDictValue() const {
  base::Value::Dict result = BaseToDictValue();
  base::Value::List revoked_origins;
  for (const auto& permission : revoked_permissions_) {
    revoked_origins.Append(permission.primary_pattern.ToString());
  }
  result.Set(kRevokedPermissionsResultKey, std::move(revoked_origins));
  return result;
}

bool RevokedPermissionsResult::IsTriggerForMenuNotification() const {
  // A menu notification should be shown when there is at least one permission
  // that was revoked.
  return !GetRevokedOrigins().empty();
}

bool RevokedPermissionsResult::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::set<ContentSettingsPattern> old_origins;
  for (const base::Value& origin_val :
       *previous_result_dict.FindList(kRevokedPermissionsResultKey)) {
    // Before crrev.com/c/5000387, the revoked permissions were stored in a dict
    // that looked as follows:
    // {
    //    "origin": "site.com",
    //    "permissionTypes": [...permissions],
    //    "expiration": TimeValue
    // }
    // After this CL, the list was updated to a list of strings representing
    // the origins. To maintain backwards compatibility, support these
    // old values for now. This check can be deleted in the future.
    const std::string* origin_str{};
    if (origin_val.is_dict()) {
      const base::Value::Dict& revoked_permission = origin_val.GetDict();
      origin_str = revoked_permission.FindString(kSafetyHubOriginKey);
    } else if (origin_val.is_string()) {
      origin_str = &origin_val.GetString();
    } else {
      NOTREACHED();
    }
    ContentSettingsPattern origin =
        ContentSettingsPattern::FromString(*origin_str);
    old_origins.insert(origin);
  }

  std::set<ContentSettingsPattern> new_origins = GetRevokedOrigins();
  return !std::ranges::includes(old_origins, new_origins);
}

std::u16string RevokedPermissionsResult::GetNotificationString() const {
  if (revoked_permissions_.empty()) {
    return std::u16string();
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_REVOKED_PERMISSIONS_MENU_NOTIFICATION,
      revoked_permissions_.size());
}

int RevokedPermissionsResult::GetNotificationCommandId() const {
  return IDC_OPEN_SAFETY_HUB;
}
