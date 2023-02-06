// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/unused_site_permissions_service.h"
#include "url/gurl.h"

namespace {
// Reflects the maximum number of days between a permissions being revoked and
// the time when the user regrants the permission through the unused site
// permission module of Safete Check. The maximum number of days is determined
// by `kRevocationCleanUpThreshold`.
size_t kAllowAgainMetricsExclusiveMaxCount = 31;
// Using a single bucket per day, following the value of
// |kAllowAgainMetricsExclusiveMaxCount|.
size_t kAllowAgainMetricsBuckets = 31;
// Key of the expiration time in the |UnusedSitePermissions| object. Indicates
// the time after which the associated origin and permissions are no longer
// shown in the UI.
constexpr char kExpirationKey[] = "expiration";
}  // namespace

SiteSettingsPermissionsHandler::SiteSettingsPermissionsHandler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}
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
  CHECK(args[0].is_string());
  const std::string& origin_str = args[0].GetString();

  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);

  url::Origin origin = url::Origin::Create(GURL(origin_str));

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_settings::SettingInfo info;
  base::Value stored_value(hcsm->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS, &info));
  base::Time revoked_time =
      info.metadata.expiration - permissions::kRevocationCleanUpThreshold;
  base::UmaHistogramCustomCounts(
      "Settings.SafetyCheck.UnusedSitePermissionsAllowAgainDays",
      (clock_->Now() - revoked_time).InDays(), 0,
      kAllowAgainMetricsExclusiveMaxCount, kAllowAgainMetricsBuckets);

  service->RegrantPermissionsForOrigin(origin);
  SendUnusedSitePermissionsReviewList();
}

void SiteSettingsPermissionsHandler::
    HandleUndoAllowPermissionsAgainForUnusedSite(
        const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_dict());

  auto [origin, permissions, constraints] =
      GetUnusedSitePermissionsFromDict(args[0].GetDict());
  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);

  service->UndoRegrantPermissionsForOrigin(permissions, constraints, origin);

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

void SiteSettingsPermissionsHandler::
    HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
        const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_list());

  const base::Value::List& unused_site_permissions_list = args[0].GetList();
  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);

  for (const auto& unused_site_permissions_js : unused_site_permissions_list) {
    CHECK(unused_site_permissions_js.is_dict());
    auto [origin, permissions, constraints] =
        GetUnusedSitePermissionsFromDict(unused_site_permissions_js.GetDict());

    service->StorePermissionInRevokedPermissionSetting(permissions, constraints,
                                                       origin);
  }

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
    DCHECK(stored_value.GetDict().FindList(permissions::kRevokedKey));

    auto type_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey)->Clone();
    base::Value::List permissions_value_list;
    for (base::Value& type : type_list) {
      permissions_value_list.Append(
          site_settings::ContentSettingsTypeToGroupName(
              static_cast<ContentSettingsType>(type.GetInt())));
    }

    revoked_permission_value.Set(
        site_settings::kPermissions,
        base::Value(std::move(permissions_value_list)));

    revoked_permission_value.Set(
        kExpirationKey,
        base::TimeToValue(revoked_permissions.metadata.expiration));

    result.Append(std::move(revoked_permission_value));
  }
  return result;
}

std::tuple<url::Origin,
           std::set<ContentSettingsType>,
           content_settings::ContentSettingConstraints>
SiteSettingsPermissionsHandler::GetUnusedSitePermissionsFromDict(
    const base::Value::Dict& unused_site_permissions) {
  const std::string* origin_str =
      unused_site_permissions.FindString(site_settings::kOrigin);
  CHECK(origin_str);
  const auto url = GURL(*origin_str);
  CHECK(url.is_valid());
  const url::Origin origin = url::Origin::Create(url);

  const base::Value::List* permissions =
      unused_site_permissions.FindList(site_settings::kPermissions);
  CHECK(permissions);
  std::set<ContentSettingsType> permission_types;
  for (const auto& permission : *permissions) {
    CHECK(permission.is_string());
    const std::string& type = permission.GetString();
    permission_types.insert(
        site_settings::ContentSettingsTypeFromGroupName(type));
  }

  const base::Value* js_expiration =
      unused_site_permissions.Find(kExpirationKey);
  CHECK(js_expiration);
  auto expiration = base::ValueToTime(js_expiration);

  const content_settings::ContentSettingConstraints constraints{
      .expiration = *expiration};

  return std::make_tuple(origin, permission_types, constraints);
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
      "undoAllowPermissionsAgainForUnusedSite",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleUndoAllowPermissionsAgainForUnusedSite,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "acknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleAcknowledgeRevokedUnusedSitePermissionsList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoAcknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SiteSettingsPermissionsHandler::
              HandleUndoAcknowledgeRevokedUnusedSitePermissionsList,
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

void SiteSettingsPermissionsHandler::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void SiteSettingsPermissionsHandler::OnJavascriptAllowed() {}

void SiteSettingsPermissionsHandler::OnJavascriptDisallowed() {}
