// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"

#include "base/json/values_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr char kRevokedPermissionsKey[] = "revoked";

SiteSettingsPermissionsHandler::SiteSettingsPermissionsHandler(Profile* profile)
    : profile_(profile) {}
SiteSettingsPermissionsHandler::~SiteSettingsPermissionsHandler() = default;

void SiteSettingsPermissionsHandler::HandleGetRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  base::Value::List result = PopulateUnusedSitePermissionsData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

base::Value::List
SiteSettingsPermissionsHandler::PopulateUnusedSitePermissionsData() {
  base::Value::List result;

  if (!base::FeatureList::IsEnabled(
          content_settings::features::kSafetyCheckUnusedSitePermissions)) {
    return result;
  }

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  ContentSettingsForOneType settings;
  hcsm->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &settings);

  for (const auto& revoked_permissions : settings) {
    base::Value::Dict revoked_permission_value;

    GURL url = GURL(revoked_permissions.primary_pattern.ToString());
    // Converting URL to a origin is normally an anti-pattern but here it is
    // ok since the URL belongs to a single origin. Therefore, it has a
    // fully defined URL+scheme+port which makes converting URL to origin
    // successful.
    url::Origin origin = url::Origin::Create(url);
    revoked_permission_value.Set(site_settings::kOrigin, origin.Serialize());

    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());

    revoked_permission_value.Set(
        kRevokedPermissionsKey,
        stored_value.GetDict().FindList(kRevokedPermissionsKey)->Clone());

    result.Append(std::move(revoked_permission_value));
  }
  return result;
}

void SiteSettingsPermissionsHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getRevokedUnusedSitePermissionsList",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleGetRevokedUnusedSitePermissionsList,
                          base::Unretained(this)));
}

void SiteSettingsPermissionsHandler::OnJavascriptAllowed() {}

void SiteSettingsPermissionsHandler::OnJavascriptDisallowed() {}
