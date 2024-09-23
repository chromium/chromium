// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/safety_hub/card_data_helper.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest.h"
#include "safety_hub_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using safety_hub::SafetyHubCardState;

namespace {

// Get values from |UnusedSitePermission| object in
// safety_hub_browser_proxy.ts.
PermissionsData GetUnusedSitePermissionsFromDict(
    const base::Value::Dict& unused_site_permissions) {
  PermissionsData permissions_data;
  const std::string* origin_str =
      unused_site_permissions.FindString(site_settings::kOrigin);
  CHECK(origin_str);
  permissions_data.primary_pattern =
      ContentSettingsPattern::FromString(*origin_str);

  const base::Value::List* permissions =
      unused_site_permissions.FindList(site_settings::kPermissions);
  CHECK(permissions);
  for (const auto& permission : *permissions) {
    CHECK(permission.is_string());
    const std::string& type_string = permission.GetString();
    ContentSettingsType type =
        site_settings::ContentSettingsTypeFromGroupName(type_string);
    CHECK(type != ContentSettingsType::DEFAULT)
        << type_string << " is not expected to have a UI representation.";
    permissions_data.permission_types.insert(type);
  }

  const base::Value::Dict* chooser_permissions_data =
      unused_site_permissions.FindDict(
          safety_hub::kSafetyHubChooserPermissionsData);
  permissions_data.chooser_permissions_data =
      chooser_permissions_data ? chooser_permissions_data->Clone()
                               : base::Value::Dict();

  // Handle expiration and lifetime for both revoked unused permissions and
  // revoked abusive notifications.
  std::vector<std::tuple<std::string, std::string,
                         content_settings::ContentSettingConstraints*>>
      keys = {{safety_hub::kExpirationKey, safety_hub::kLifetimeKey,
               &permissions_data.constraints}};
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation)) {
    keys.push_back({safety_hub::kAbusiveRevocationExpirationKey,
                    safety_hub::kAbusiveRevocationLifetimeKey,
                    &permissions_data.abusive_revocation_constraints});
  }
  for (const auto& [expiration_key, lifetime_key, constraints] : keys) {
    const base::Value* js_expiration =
        unused_site_permissions.Find(expiration_key);
    CHECK(js_expiration);
    base::Time expiration = base::ValueToTime(js_expiration).value();

    const base::Value* js_lifetime = unused_site_permissions.Find(lifetime_key);
    base::TimeDelta lifetime = content_settings::RuleMetaData::ComputeLifetime(
        /*lifetime=*/
        base::ValueToTimeDelta(js_lifetime).value_or(base::TimeDelta()),
        /*expiration=*/expiration);
    *constraints =
        content_settings::ContentSettingConstraints(expiration - lifetime);
    constraints->set_lifetime(lifetime);
  }

  return permissions_data;
}

// Returns true if the card dict indicates there is something actionable for the
// user.
bool CardHasRecommendations(base::Value::Dict card_data) {
  std::optional<int> state = card_data.FindInt(safety_hub::kCardStateKey);
  CHECK(state.has_value());
  SafetyHubCardState card_state =
      static_cast<SafetyHubCardState>(state.value());

  return card_state == SafetyHubCardState::kWarning ||
         card_state == SafetyHubCardState::kWeak;
}

void AppendModuleNameToString(std::u16string& str,
                              int uppercase_id,
                              int lowercase_id = 0) {
  if (str.empty()) {
    str.append(l10n_util::GetStringUTF16(uppercase_id));
    return;
  }

  if (lowercase_id == 0) {
    lowercase_id = uppercase_id;
  }

  str.append(
      l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR));
  str.append(u" ");
  str.append(l10n_util::GetStringUTF16(lowercase_id));
}

// Converts the entry point data into a base::Value::Dict.
base::Value::Dict EntryPointDataToValue(bool has_recommendations,
                                        std::string header,
                                        std::string subheader) {
  base::Value::Dict dict_data;

  dict_data.Set("hasRecommendations", has_recommendations);
  dict_data.Set("header", header);
  dict_data.Set("subheader", subheader);

  return dict_data;
}
}  // namespace

SafetyHubHandler::SafetyHubHandler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  prefs_observation_.Observe(ExtensionPrefs::Get(profile_));
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}
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

  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  url::Origin origin = url::Origin::Create(GURL(origin_str));

  service->RegrantPermissionsForOrigin(origin);
  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAllowPermissionsAgainForUnusedSite(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_dict());

  PermissionsData permissions_data =
      GetUnusedSitePermissionsFromDict(args[0].GetDict());
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  service->UndoRegrantPermissionsForOrigin(permissions_data);

  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleAcknowledgeRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);
  service->ClearRevokedPermissionsList();

  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_list());

  const base::Value::List& unused_site_permissions_list = args[0].GetList();
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& unused_site_permissions_js : unused_site_permissions_list) {
    CHECK(unused_site_permissions_js.is_dict());
    PermissionsData permissions_data =
        GetUnusedSitePermissionsFromDict(unused_site_permissions_js.GetDict());
    if (base::FeatureList::IsEnabled(
            safe_browsing::kSafetyHubAbusiveNotificationRevocation)) {
      HostContentSettingsMap* map =
          HostContentSettingsMapFactory::GetForProfile(profile_);
      // This pattern is origin-scoped, so this conversion is safe.
      GURL permission_url =
          permissions_data.primary_pattern.ToRepresentativeUrl();
      DCHECK(permission_url.is_valid());
      // If the permission_types includes `NOTIFICATIONS`, then the revocation
      // is for a site that should have a
      // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting.
      if (permissions_data.permission_types.contains(
              ContentSettingsType::NOTIFICATIONS)) {
        safety_hub_util::SetRevokedAbusiveNotificationPermission(
            map, permission_url, /*is_ignored=*/false,
            permissions_data.abusive_revocation_constraints);
        // Remove `NOTIFICATIONS` from permission type list for handling unused
        // permission revocation below.
        permissions_data.permission_types.erase(
            ContentSettingsType::NOTIFICATIONS);
      }

      // If the permission_types include any permission type that is not
      // `NOTIFICATIONS`, then the revocation is for an unused site that should
      // have a `REVOKED_UNUSED_SITE_PERMISSIONS` setting.
      if (!permissions_data.permission_types.empty()) {
        service->StorePermissionInRevokedPermissionSetting(permissions_data);
      }
    } else {
      service->StorePermissionInRevokedPermissionSetting(permissions_data);
    }
  }

  SendUnusedSitePermissionsReviewList();
}

base::Value::List SafetyHubHandler::PopulateUnusedSitePermissionsData() {
  base::Value::List result;
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kSafetyCheckUnusedSitePermissions) &&
      !base::FeatureList::IsEnabled(
          safe_browsing::kSafetyHubAbusiveNotificationRevocation)) {
    return result;
  }

  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);
  std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
      service_result = service->GetRevokedPermissions();
  for (const auto& permissions_data : service_result->GetRevokedPermissions()) {
    base::Value::Dict revoked_permission_value;
    revoked_permission_value.Set(site_settings::kOrigin,
                                 permissions_data.primary_pattern.ToString());

    base::Value::List permissions_value_list;
    for (ContentSettingsType type : permissions_data.permission_types) {
      std::string_view permission_str =
          site_settings::ContentSettingsTypeToGroupName(type);
      if (!permission_str.empty()) {
        permissions_value_list.Append(permission_str);
      }
    }

    // Some permissions have no readable name, although Safety Hub revokes them.
    // To prevent crashes, if there is no permission to be shown in the UI, the
    // origin will not be added to the revoked permissions list.
    // TODO(crbug.com/40066645): Remove this after adding check for
    // ContentSettingsTypeToGroupName.
    if (permissions_value_list.empty()) {
      continue;
    }

    revoked_permission_value.Set(
        site_settings::kPermissions,
        base::Value(std::move(permissions_value_list)));

    revoked_permission_value.Set(
        safety_hub::kExpirationKey,
        base::TimeToValue(permissions_data.constraints.expiration()));

    revoked_permission_value.Set(
        safety_hub::kLifetimeKey,
        base::TimeDeltaToValue(permissions_data.constraints.lifetime()));

    revoked_permission_value.Set(
        safety_hub::kSafetyHubChooserPermissionsData,
        base::Value(permissions_data.chooser_permissions_data.Clone()));

    if (base::FeatureList::IsEnabled(
            safe_browsing::kSafetyHubAbusiveNotificationRevocation)) {
      revoked_permission_value.Set(
          safety_hub::kAbusiveRevocationExpirationKey,
          base::TimeToValue(
              permissions_data.abusive_revocation_constraints.expiration()));

      revoked_permission_value.Set(
          safety_hub::kAbusiveRevocationLifetimeKey,
          base::TimeDeltaToValue(
              permissions_data.abusive_revocation_constraints.lifetime()));
    }

    result.Append(std::move(revoked_permission_value));
  }

  return result;
}

void SafetyHubHandler::HandleGetNotificationPermissionReviewList(
    const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  if (!service) {
    RejectJavascriptCallback(callback_id, base::Value());
  }

  base::Value::List result =
      service->PopulateNotificationPermissionReviewData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SafetyHubHandler::HandleIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

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

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    service->SetNotificationPermissionsForOrigin(origin.GetString(),
                                                 CONTENT_SETTING_DEFAULT);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleDismissActiveMenuNotification(
    const base::Value::List& args) {
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile_)
      ->DismissActiveNotification();
}

void SafetyHubHandler::HandleDismissPasswordMenuNotification(
    const base::Value::List& args) {
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile_)
      ->DismissActiveNotificationOfModule(
          safety_hub::SafetyHubModuleType::PASSWORDS);
}

void SafetyHubHandler::HandleDismissExtensionsMenuNotification(
    const base::Value::List& args) {
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile_)
      ->DismissActiveNotificationOfModule(
          safety_hub::SafetyHubModuleType::EXTENSIONS);
}

void SafetyHubHandler::HandleBlockNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    service->SetNotificationPermissionsForOrigin(origin.GetString(),
                                                 CONTENT_SETTING_BLOCK);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleAllowNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    service->SetNotificationPermissionsForOrigin(origin.GetString(),
                                                 CONTENT_SETTING_ALLOW);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleUndoIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern& primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->RemovePatternFromNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }
  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleGetSafeBrowsingCardData(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id,
                            safety_hub::GetSafeBrowsingCardData(profile_));
}

void SafetyHubHandler::HandleGetNumberOfExtensionsThatNeedReview(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id,
                            base::Value(GetNumberOfExtensionsThatNeedReview()));
}

void SafetyHubHandler::HandleGetPasswordCardData(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(
      callback_id, base::Value(safety_hub::GetPasswordCardData(profile_)));
}

void SafetyHubHandler::HandleGetVersionCardData(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id,
                            base::Value(safety_hub::GetVersionCardData()));
}

void SafetyHubHandler::HandleGetSafetyHubEntryPointData(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  std::set<SafetyHubModule> modules = GetSafetyHubModulesWithRecommendations();

  // If there is no module that needs attention, a static string will be used
  // for the subheader.
  if (modules.empty()) {
    ResolveJavascriptCallback(
        callback_id,
        base::Value(EntryPointDataToValue(
            false, "",
            l10n_util::GetStringUTF8(
                IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_NOTHING_TO_DO))));
    return;
  }

  // Modules in subheader should be added in the following order: Passwords,
  // Version, Safe Browsing, Extensions, Notifications, Permissions.
  std::u16string subheader = u"";

  if (modules.contains(SafetyHubModule::kPasswords)) {
    AppendModuleNameToString(subheader,
                             IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME);
  }

  if (modules.contains(SafetyHubModule::kVersion)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kSafeBrowsing)) {
    AppendModuleNameToString(subheader,
                             IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MODULE_NAME);
  }

  if (modules.contains(SafetyHubModule::kExtensions)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kNotifications)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kUnusedSitePermissions)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_LOWERCASE_NAME);
  }

  ResolveJavascriptCallback(
      callback_id,
      base::Value(EntryPointDataToValue(
          true,
          l10n_util::GetStringUTF8(IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_HEADER),
          base::UTF16ToUTF8(subheader))));
}

std::set<SafetyHubHandler::SafetyHubModule>
SafetyHubHandler::GetSafetyHubModulesWithRecommendations() {
  std::set<SafetyHubModule> modules;

  // Passwords module
  if (CardHasRecommendations(safety_hub::GetPasswordCardData(profile_))) {
    modules.insert(SafetyHubModule::kPasswords);
  }
  // Version module
  if (CardHasRecommendations(safety_hub::GetVersionCardData())) {
    modules.insert(SafetyHubModule::kVersion);
  }
  // SafeBrowsing module
  if (CardHasRecommendations(safety_hub::GetSafeBrowsingCardData(profile_))) {
    modules.insert(SafetyHubModule::kSafeBrowsing);
  }
  // Extensions module
  if (GetNumberOfExtensionsThatNeedReview() > 0) {
    modules.insert(SafetyHubModule::kExtensions);
  }
  // Notifications module
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);
  if (!service->PopulateNotificationPermissionReviewData().empty()) {
    modules.insert(SafetyHubModule::kNotifications);
  }
  // Unused site permission module
  if (!PopulateUnusedSitePermissionsData().empty()) {
    modules.insert(SafetyHubModule::kUnusedSitePermissions);
  }

  return modules;
}

void SafetyHubHandler::HandleRecordSafetyHubVisit(
    const base::Value::List& args) {
  if (SafetyHubHatsService* hats_service =
          SafetyHubHatsServiceFactory::GetForProfile(profile_)) {
    hats_service->SafetyHubVisited();
  }
}

void SafetyHubHandler::HandleRecordSafetyHubInteraction(
    const base::Value::List& args) {
  if (SafetyHubHatsService* hats_service =
          SafetyHubHatsServiceFactory::GetForProfile(profile_)) {
    hats_service->SafetyHubModuleInteracted();
  }
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
      "dismissActiveMenuNotification",
      base::BindRepeating(
          &SafetyHubHandler::HandleDismissActiveMenuNotification,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dismissSafetyHubPasswordMenuNotification",
      base::BindRepeating(
          &SafetyHubHandler::HandleDismissPasswordMenuNotification,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dismissSafetyHubExtensionsMenuNotification",
      base::BindRepeating(
          &SafetyHubHandler::HandleDismissExtensionsMenuNotification,
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
  web_ui()->RegisterMessageCallback(
      "getSafeBrowsingCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetSafeBrowsingCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPasswordCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetPasswordCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getVersionCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetVersionCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSafetyHubEntryPointData",
      base::BindRepeating(&SafetyHubHandler::HandleGetSafetyHubEntryPointData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNumberOfExtensionsThatNeedReview",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetNumberOfExtensionsThatNeedReview,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordSafetyHubPageVisit",
      base::BindRepeating(&SafetyHubHandler::HandleRecordSafetyHubVisit,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordSafetyHubInteraction",
      base::BindRepeating(&SafetyHubHandler::HandleRecordSafetyHubInteraction,
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
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  // Notify observers that the permission review list could have changed. Note
  // that the list is not guaranteed to have changed.
  FireWebUIListener(
      site_settings::kNotificationPermissionsReviewListMaybeChangedEvent,
      service->PopulateNotificationPermissionReviewData());
}

void SafetyHubHandler::InitSafetyHubExtensionResults() {
  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(profile_, false);
  if (sh_result.has_value()) {
    extension_sh_result_ = std::make_unique<SafetyHubExtensionsResult>(
        *static_cast<SafetyHubExtensionsResult*>(sh_result->get()));
  }
}

int SafetyHubHandler::GetNumberOfExtensionsThatNeedReview() {
  if (!extension_sh_result_) {
    InitSafetyHubExtensionResults();
  }
  if (extension_sh_result_) {
    return extension_sh_result_->GetNumTriggeringExtensions();
  } else {
    return 0;
  }
}

void SafetyHubHandler::UpdateNumberOfExtensionsThatNeedReview(
    int num_extension_need_review_before,
    int num_extension_need_review_after) {
  if (num_extension_need_review_before != num_extension_need_review_after) {
    AllowJavascript();
    FireWebUIListener("extensions-review-list-maybe-changed",
                      num_extension_need_review_after);
  }
}

void SafetyHubHandler::OnExtensionPrefsUpdated(
    const std::string& extension_id) {
  if (!extension_sh_result_) {
    return;
  }
  int num_extension_need_review_before = GetNumberOfExtensionsThatNeedReview();
  extension_sh_result_->OnExtensionPrefsUpdated(extension_id, profile_);
  int num_extension_need_review_after = GetNumberOfExtensionsThatNeedReview();
  UpdateNumberOfExtensionsThatNeedReview(num_extension_need_review_before,
                                         num_extension_need_review_after);
}

void SafetyHubHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (!extension_sh_result_) {
    return;
  }
  int num_extension_need_review_before = GetNumberOfExtensionsThatNeedReview();
  extension_sh_result_->OnExtensionUninstalled(browser_context, extension,
                                               reason);
  int num_extension_need_review_after = GetNumberOfExtensionsThatNeedReview();
  UpdateNumberOfExtensionsThatNeedReview(num_extension_need_review_before,
                                         num_extension_need_review_after);
}

void SafetyHubHandler::OnExtensionPrefsWillBeDestroyed(ExtensionPrefs* prefs) {
  DCHECK(prefs_observation_.IsObservingSource(prefs));
  prefs_observation_.Reset();
}

void SafetyHubHandler::OnShutdown(extensions::ExtensionRegistry* registry) {
  extension_registry_observation_.Reset();
}

void SafetyHubHandler::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void SafetyHubHandler::ClearExtensionResultsForTesting() {
  GetNumberOfExtensionsThatNeedReview();
  if (extension_sh_result_) {
    extension_sh_result_->ClearTriggeringExtensionsForTesting();  // IN-TEST
  }
}

void SafetyHubHandler::SetTriggeringExtensionForTesting(
    std::string extension_id) {
  GetNumberOfExtensionsThatNeedReview();
  if (extension_sh_result_) {
    extension_sh_result_->SetTriggeringExtensionForTesting(  // IN-TEST
        extension_id);                                       // IN-TEST
  }
}

void SafetyHubHandler::OnJavascriptAllowed() {}

void SafetyHubHandler::OnJavascriptDisallowed() {}
