// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_manager.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kUnknownContentSettingsType[] = "unknown";

base::Value::List ConvertContentSettingsIntValuesToString(
    const base::Value::List& content_settings_values_list,
    bool* successful_migration) {
  base::Value::List string_value_list;
  for (const base::Value& setting_value : content_settings_values_list) {
    if (setting_value.is_int()) {
      int setting_int = setting_value.GetInt();
      auto setting_name =
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(setting_int));
      if (setting_name == kUnknownContentSettingsType) {
        *successful_migration = false;
        string_value_list.Append(setting_value.GetInt());
      } else {
        string_value_list.Append(setting_name);
      }
    } else {
      DCHECK(setting_value.is_string());
      // Store string group name values.
      string_value_list.Append(setting_value.GetString());
    }
  }
  return string_value_list;
}

base::Value::Dict ConvertChooserContentSettingsIntValuesToString(
    const base::Value::Dict& chooser_content_settings_values_dict) {
  base::Value::Dict string_keyed_dict;
  for (const auto [key, value] : chooser_content_settings_values_dict) {
    int number = -1;
    base::StringToInt(key, &number);
    // If number conversion fails it returns 0 which is not a chooser permission
    // enum value so it will not clash with the conversion.
    if (number == 0) {
      // Store string keyed values as is.
      string_keyed_dict.Set(key, value.GetDict().Clone());
    } else {
      string_keyed_dict.Set(
          UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
              static_cast<ContentSettingsType>(number)),
          value.GetDict().Clone());
    }
  }
  return string_keyed_dict;
}

}  // namespace

// static
std::string UnusedSitePermissionsManager::ConvertContentSettingsTypeToKey(
    ContentSettingsType type) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  DCHECK(website_setting_registry);

  auto* website_settings_info = website_setting_registry->Get(type);
  if (!website_settings_info) {
    auto integer_type = static_cast<int32_t>(type);
    DVLOG(1) << "Couldn't retrieve website settings info entry from the "
                "registry for type: "
             << integer_type;
    base::UmaHistogramSparse(
        "Settings.SafetyCheck.UnusedSitePermissionsMigrationFail",
        integer_type);
    return kUnknownContentSettingsType;
  }

  return website_settings_info->name();
}

// static
ContentSettingsType
UnusedSitePermissionsManager::ConvertKeyToContentSettingsType(
    const std::string& key) {
  auto* website_setting_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  return website_setting_registry->GetByName(key)->type();
}

// static
url::Origin UnusedSitePermissionsManager::ConvertPrimaryPatternToOrigin(
    const ContentSettingsPattern& primary_pattern) {
  GURL origin_url = GURL(primary_pattern.ToString());
  CHECK(origin_url.is_valid());

  return url::Origin::Create(origin_url);
}

UnusedSitePermissionsManager::UnusedSitePermissionsManager(
    content::BrowserContext* browser_context,
    PrefService* prefs)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  bool migration_completed = pref_change_registrar_->prefs()->GetBoolean(
      safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted);
  if (!migration_completed) {
    // Convert all integer permission values to string, if there is any
    // permission represented by integer stored in disk.
    // TODO(crbug.com/415227458): Clean up this migration after some milestones.
    UpdateIntegerValuesToGroupName();
  }
}

UnusedSitePermissionsManager::~UnusedSitePermissionsManager() = default;

void UnusedSitePermissionsManager::UpdateIntegerValuesToGroupName() {
  ContentSettingsForOneType settings = hcsm()->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);

  bool successful_migration = true;
  for (const auto& revoked_permissions : settings) {
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());
    base::Value updated_dict(stored_value.Clone());

    const base::Value::List* permission_value_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey);
    if (permission_value_list) {
      base::Value::List updated_permission_value_list =
          ConvertContentSettingsIntValuesToString(
              permission_value_list->Clone(), &successful_migration);
      updated_dict.GetDict().Set(permissions::kRevokedKey,
                                 std::move(updated_permission_value_list));
    }

    const base::Value::Dict* chooser_permission_value_dict =
        stored_value.GetDict().FindDict(
            permissions::kRevokedChooserPermissionsKey);
    if (chooser_permission_value_dict) {
      base::Value::Dict updated_chooser_permission_value_dict =
          ConvertChooserContentSettingsIntValuesToString(
              chooser_permission_value_dict->Clone());
      updated_dict.GetDict().Set(
          permissions::kRevokedChooserPermissionsKey,
          std::move(updated_chooser_permission_value_dict));
    }

    // Create a new constraint with the old creation time of the original
    // exception.
    base::Time creation_time = revoked_permissions.metadata.expiration() -
                               revoked_permissions.metadata.lifetime();
    content_settings::ContentSettingConstraints constraints(creation_time);
    constraints.set_lifetime(revoked_permissions.metadata.lifetime());

    hcsm()->SetWebsiteSettingCustomScope(
        revoked_permissions.primary_pattern,
        revoked_permissions.secondary_pattern,
        ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
        std::move(updated_dict), constraints);
  }

  if (successful_migration) {
    pref_change_registrar_->prefs()->SetBoolean(
        safety_hub_prefs::kUnusedSitePermissionsRevocationMigrationCompleted,
        true);
  }
}
