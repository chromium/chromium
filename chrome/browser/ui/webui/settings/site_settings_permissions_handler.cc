// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"

#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/unused_site_permissions_service.h"
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

void SiteSettingsPermissionsHandler::HandleAllowPermissionsAgainForUnusedSite(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& origin_str = args[0].GetString();

  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);

  url::Origin origin = url::Origin::Create(GURL(origin_str));
  service->RegrantPermissionsForOrigin(origin);

  SendUnusedSitePermissionsReviewList();
}

void SiteSettingsPermissionsHandler::
    HandleAcknowledgeRevokedUnusedSitePermissionsList(
        const base::Value::List& args) {
  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  service->ClearRevokedPermissionsList();

  SendUnusedSitePermissionsReviewList();
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
    revoked_permission_value.Set(
        site_settings::kOrigin, revoked_permissions.primary_pattern.ToString());
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());

    // The revoked permissions list should be reachable by given key.
    DCHECK(stored_value.GetDict().FindList(kRevokedPermissionsKey));

    auto type_list =
        stored_value.GetDict().FindList(kRevokedPermissionsKey)->Clone();
    base::Value::List permissions_value_list;
    for (base::Value& type : type_list) {
      permissions_value_list.Append(
          site_settings::ContentSettingsTypeToGroupName(
              static_cast<ContentSettingsType>(type.GetInt())));
    }

    revoked_permission_value.Set(
        site_settings::kPermissions,
        base::Value(std::move(permissions_value_list)));

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
  web_ui()->RegisterMessageCallback(
      "allowPermissionsAgainForUnusedSite",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleAllowPermissionsAgainForUnusedSite,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "acknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleAcknowledgeRevokedUnusedSitePermissionsList,
                          base::Unretained(this)));
}

void SiteSettingsPermissionsHandler::SendUnusedSitePermissionsReviewList() {
  // Notify observers that the unused site permission review list could have
  // changed. Note that the list is not guaranteed to have changed. In places
  // where determining whether the list has changed is cause for performance
  // concerns, an unchanged list may be sent.
  FireWebUIListener("unused-permission-review-list-maybe-changed",
                    PopulateUnusedSitePermissionsData());
}

void SiteSettingsPermissionsHandler::OnJavascriptAllowed() {}

void SiteSettingsPermissionsHandler::OnJavascriptDisallowed() {}
