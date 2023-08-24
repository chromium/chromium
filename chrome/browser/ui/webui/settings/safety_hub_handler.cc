// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"
#include <memory>

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/notification_permission_review_service_factory.h"
#include "chrome/browser/permissions/unused_site_permissions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/unused_site_permissions_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
// Key of the expiration time in the |UnusedSitePermissions| object. Indicates
// the time after which the associated origin and permissions are no longer
// shown in the UI.
constexpr char kExpirationKey[] = "expiration";
// Key of the lifetime in the |UnusedSitePermissions| object.
constexpr char kLifetimeKey[] = "lifetime";

// Get values from |UnusedSitePermission| object in
// safety_hub_browser_proxy.ts.
std::tuple<url::Origin,
           std::set<ContentSettingsType>,
           content_settings::ContentSettingConstraints>
GetUnusedSitePermissionsFromDict(
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
    const std::string& type_string = permission.GetString();
    ContentSettingsType type =
        site_settings::ContentSettingsTypeFromGroupName(type_string);
    CHECK(type != ContentSettingsType::DEFAULT)
        << type_string << " is not expected to have a UI representation.";
    permission_types.insert(type);
  }

  const base::Value* js_expiration =
      unused_site_permissions.Find(kExpirationKey);
  CHECK(js_expiration);
  base::Time expiration = base::ValueToTime(js_expiration).value();

  const base::Value* js_lifetime = unused_site_permissions.Find(kLifetimeKey);
  // TODO(https://crbug.com/1455435): The use of ComputeLifetime here should be
  // temporary. Once all persisted RuleMetaData instances include lifetimes, we
  // can remove this, and just use the stored lifetime directly. We can do this
  // after all lifetime-less settings have expired. Realistically this will take
  // only one or two milestones, so this can safely be removed in M118 or M119.
  base::TimeDelta lifetime = content_settings::RuleMetaData::ComputeLifetime(
      /*lifetime=*/js_lifetime ? base::ValueToTimeDelta(js_lifetime).value()
                               : base::TimeDelta(),
      /*expiration=*/expiration);

  content_settings::ContentSettingConstraints constraints =
      content_settings::ContentSettingConstraints(expiration - lifetime);
  constraints.set_lifetime(lifetime);

  return std::make_tuple(origin, permission_types, constraints);
}
}  // namespace

SafetyHubHandler::SafetyHubHandler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}
SafetyHubHandler::~SafetyHubHandler() = default;

// static
std::unique_ptr<SafetyHubHandler> SafetyHubHandler::GetForProfile(
    Profile* profile) {
  return std::make_unique<SafetyHubHandler>(profile);
}

void SafetyHubHandler::HandleGetRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  base::Value::List result = PopulateUnusedSitePermissionsData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SafetyHubHandler::HandleAllowPermissionsAgainForUnusedSite(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_string());
  const std::string& origin_str = args[0].GetString();

  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);

  url::Origin origin = url::Origin::Create(GURL(origin_str));

  service->RegrantPermissionsForOrigin(origin);
  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAllowPermissionsAgainForUnusedSite(
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

void SafetyHubHandler::HandleAcknowledgeRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  permissions::UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  service->ClearRevokedPermissionsList();

  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
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

base::Value::List SafetyHubHandler::PopulateUnusedSitePermissionsData() {
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
      base::StringPiece permission_str =
          site_settings::ContentSettingsTypeToGroupName(
              static_cast<ContentSettingsType>(type.GetInt()));
      if (!permission_str.empty()) {
        permissions_value_list.Append(permission_str);
      }
    }

    // Some permissions have no readable name, although Safety Hub revokes them.
    // To prevent crashes, if there is no permission to be shown in the UI, the
    // origin will not be added to the revoked permissions list.
    // TODO(crbug.com/1459305): Remove this after adding check for
    // ContentSettingsTypeToGroupName.
    if (permissions_value_list.empty()) {
      continue;
    }

    revoked_permission_value.Set(
        site_settings::kPermissions,
        base::Value(std::move(permissions_value_list)));

    revoked_permission_value.Set(
        kExpirationKey,
        base::TimeToValue(revoked_permissions.metadata.expiration()));

    revoked_permission_value.Set(
        kLifetimeKey,
        base::TimeDeltaToValue(revoked_permissions.metadata.lifetime()));

    result.Append(std::move(revoked_permission_value));
  }
  return result;
}

void SafetyHubHandler::HandleGetNotificationPermissionReviewList(
    const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];

  base::Value::List result =
      site_settings::PopulateNotificationPermissionReviewData(profile_);

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SafetyHubHandler::HandleIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->AddPatternToNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleResetNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_DEFAULT);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleBlockNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_BLOCK);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleAllowNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_ALLOW);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleUndoIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern& primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->RemovePatternFromNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }
  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "allowPermissionsAgainForUnusedSite",
      base::BindRepeating(
          &SafetyHubHandler::HandleAllowPermissionsAgainForUnusedSite,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoAllowPermissionsAgainForUnusedSite",
      base::BindRepeating(
          &SafetyHubHandler::HandleUndoAllowPermissionsAgainForUnusedSite,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "acknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::HandleAcknowledgeRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoAcknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::
              HandleUndoAcknowledgeRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationPermissionReview",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetNotificationPermissionReviewList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "ignoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleIgnoreOriginsForNotificationPermissionReview,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleResetNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "blockNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleBlockNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "allowNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleAllowNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoIgnoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::
              HandleUndoIgnoreOriginsForNotificationPermissionReview,
          base::Unretained(this)));
}

void SafetyHubHandler::SendUnusedSitePermissionsReviewList() {
  // Notify observers that the unused site permission review list could have
  // changed. Note that the list is not guaranteed to have changed. In places
  // where determining whether the list has changed is cause for performance
  // concerns, an unchanged list may be sent.
  FireWebUIListener("unused-permission-review-list-maybe-changed",
                    PopulateUnusedSitePermissionsData());
}

void SafetyHubHandler::SendNotificationPermissionReviewList() {
  // Notify observers that the permission review list could have changed. Note
  // that the list is not guaranteed to have changed.
  FireWebUIListener(
      site_settings::kNotificationPermissionsReviewListMaybeChangedEvent,
      site_settings::PopulateNotificationPermissionReviewData(profile_));
}

void SafetyHubHandler::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void SafetyHubHandler::OnJavascriptAllowed() {}

void SafetyHubHandler::OnJavascriptDisallowed() {}
