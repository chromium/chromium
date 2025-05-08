// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <optional>
#include <ostream>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const DataType canonical_data_type;
  const DataTypeSet data_type_group;
};

constexpr char kBookmarksTypeName[] = "bookmarks";
constexpr char kPreferencesTypeName[] = "preferences";
constexpr char kPasswordsTypeName[] = "passwords";
constexpr char kAutofillTypeName[] = "autofill";
constexpr char kThemesTypeName[] = "themes";
// Note: The type name for History is "typedUrls" for historic reasons. This
// name is used in JS (sync settings) and in the SyncTypesListDisabled policy,
// so it's fairly hard to change.
constexpr char kHistoryTypeName[] = "typedUrls";
constexpr char kExtensionsTypeName[] = "extensions";
constexpr char kAppsTypeName[] = "apps";
constexpr char kReadingListTypeName[] = "readingList";
constexpr char kTabsTypeName[] = "tabs";
constexpr char kSavedTabGroupsTypeName[] = "savedTabGroups";
constexpr char kPaymentsTypeName[] = "payments";
constexpr char kProductComparisonTypeName[] = "productComparison";
constexpr char kCookiesTypeName[] = "cookies";

UserSelectableTypeInfo GetUserSelectableTypeInfo(
    UserSelectableType type,
    // TODO(crbug.com/412602018): Remove this parameter once the feature is
    // launched.
    bool skip_feature_checks_if_early = false) {
  static_assert(55 == syncer::GetNumDataTypes(),
                "Almost always when adding a new Data, you must tie it to "
                "a UserSelectableType below (new or existing) so the user can "
                "disable syncing of that data. Today you must also update the "
                "UI code yourself; crbug.com/1067282 and related bugs will "
                "improve that");
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {kBookmarksTypeName, BOOKMARKS, {BOOKMARKS, POWER_BOOKMARK}};
    case UserSelectableType::kPreferences: {
      DataTypeSet types = {PREFERENCES, DICTIONARY, SEARCH_ENGINES};
      // `skip_feature_checks_if_early` is used to avoid checking the feature
      // state during early startup phase, which can happen when setting
      // policies during pref service initialization. It is only set to true
      // when called from `GetUserSelectableTypeName()` and thus, is not
      // affected by the feature flag anyway.
      // See crbug.com/415305009 for more context.
      if ((!skip_feature_checks_if_early || base::FeatureList::GetInstance()) &&
          !base::FeatureList::IsEnabled(
              kSyncSupportAlwaysSyncingPriorityPreferences)) {
        types.Put(PRIORITY_PREFERENCES);
      }
      return {kPreferencesTypeName, PREFERENCES, types};
    }
    case UserSelectableType::kPasswords:
      return {
          kPasswordsTypeName,
          PASSWORDS,
          {PASSWORDS, WEBAUTHN_CREDENTIAL, INCOMING_PASSWORD_SHARING_INVITATION,
           OUTGOING_PASSWORD_SHARING_INVITATION}};
    case UserSelectableType::kAutofill:
      return {kAutofillTypeName,
              AUTOFILL,
              {AUTOFILL, AUTOFILL_PROFILE, CONTACT_INFO}};
    case UserSelectableType::kThemes:
      return {kThemesTypeName, THEMES, {THEMES}};
    case UserSelectableType::kHistory:
      return {kHistoryTypeName,
              HISTORY,
              {HISTORY, HISTORY_DELETE_DIRECTIVES, USER_EVENTS}};
    case UserSelectableType::kExtensions:
      return {
          kExtensionsTypeName, EXTENSIONS, {EXTENSIONS, EXTENSION_SETTINGS}};
    case UserSelectableType::kApps:
#if BUILDFLAG(IS_CHROMEOS)
      // In Chrome OS, "Apps" is a sub-item of OS settings.
      return {kAppsTypeName, UNSPECIFIED};
#else
      return {kAppsTypeName, APPS, {APPS, APP_SETTINGS, WEB_APPS, WEB_APKS}};
#endif
    case UserSelectableType::kReadingList:
      return {kReadingListTypeName, READING_LIST, {READING_LIST}};
    case UserSelectableType::kTabs:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      return {kTabsTypeName,
              SESSIONS,
              {SESSIONS, SAVED_TAB_GROUP, SHARED_TAB_GROUP_DATA,
               COLLABORATION_GROUP, SHARED_TAB_GROUP_ACCOUNT_DATA}};
#else
      return {kTabsTypeName, SESSIONS, {SESSIONS}};
#endif
    case UserSelectableType::kSavedTabGroups:
      // Note: Tab groups is presented as a separate type only on desktop.
      // On mobile platforms, it is bundled together with open tabs.
      // TODO(crbug.com/361625227): In post-UNO world, it will be bundled
      // together with open tabs same as mobile.
      return {kSavedTabGroupsTypeName,
              SAVED_TAB_GROUP,
              {SAVED_TAB_GROUP, SHARED_TAB_GROUP_DATA, COLLABORATION_GROUP,
               SHARED_TAB_GROUP_ACCOUNT_DATA}};
    case UserSelectableType::kPayments:
      return {kPaymentsTypeName,
              AUTOFILL_WALLET_DATA,
              {AUTOFILL_WALLET_CREDENTIAL, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA, AUTOFILL_WALLET_OFFER,
               AUTOFILL_WALLET_USAGE, AUTOFILL_VALUABLE}};
    case UserSelectableType::kProductComparison:
      return {
          kProductComparisonTypeName, PRODUCT_COMPARISON, {PRODUCT_COMPARISON}};
    case UserSelectableType::kCookies:
      return {kCookiesTypeName, COOKIES, {COOKIES}};
  }
  NOTREACHED();
}

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kOsAppsTypeName[] = "osApps";
constexpr char kOsPreferencesTypeName[] = "osPreferences";
constexpr char kOsWifiConfigurationsTypeName[] = "osWifiConfigurations";
constexpr char kWifiConfigurationsTypeName[] = "wifiConfigurations";

UserSelectableTypeInfo GetUserSelectableOsTypeInfo(UserSelectableOsType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableOsType::kOsApps:
      return {kOsAppsTypeName,
              APPS,
              {APP_LIST, APPS, APP_SETTINGS, ARC_PACKAGE, WEB_APPS}};
    case UserSelectableOsType::kOsPreferences:
      return {kOsPreferencesTypeName,
              OS_PREFERENCES,
              {OS_PREFERENCES, OS_PRIORITY_PREFERENCES, PRINTERS,
               PRINTERS_AUTHORIZATION_SERVERS, WORKSPACE_DESK}};
    case UserSelectableOsType::kOsWifiConfigurations:
      return {kOsWifiConfigurationsTypeName,
              WIFI_CONFIGURATIONS,
              {WIFI_CONFIGURATIONS}};
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type, /*skip_feature_checks_if_early=*/true)
      .type_name;
}

std::optional<UserSelectableType> GetUserSelectableTypeFromString(
    const std::string& type) {
  if (type == kBookmarksTypeName) {
    return UserSelectableType::kBookmarks;
  }
  if (type == kPreferencesTypeName) {
    return UserSelectableType::kPreferences;
  }
  if (type == kPasswordsTypeName) {
    return UserSelectableType::kPasswords;
  }
  if (type == kAutofillTypeName) {
    return UserSelectableType::kAutofill;
  }
  if (type == kThemesTypeName) {
    return UserSelectableType::kThemes;
  }
  if (type == kHistoryTypeName) {
    return UserSelectableType::kHistory;
  }
  if (type == kExtensionsTypeName) {
    return UserSelectableType::kExtensions;
  }
  if (type == kAppsTypeName) {
    return UserSelectableType::kApps;
  }
  if (type == kReadingListTypeName) {
    return UserSelectableType::kReadingList;
  }
  if (type == kTabsTypeName) {
    return UserSelectableType::kTabs;
  }
  if (type == kSavedTabGroupsTypeName) {
    return UserSelectableType::kSavedTabGroups;
  }
  if (type == kProductComparisonTypeName) {
    return UserSelectableType::kProductComparison;
  }
  if (type == kCookiesTypeName) {
    return UserSelectableType::kCookies;
  }
  return std::nullopt;
}

std::string UserSelectableTypeSetToString(UserSelectableTypeSet types) {
  std::string result;
  for (UserSelectableType type : types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += GetUserSelectableTypeName(type);
  }
  return result;
}

DataTypeSet UserSelectableTypeToAllDataTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).data_type_group;
}

DataType UserSelectableTypeToCanonicalDataType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_data_type;
}

std::optional<UserSelectableType> GetUserSelectableTypeFromDataType(
    DataType data_type) {
  std::optional<UserSelectableType> selectable_type;

  for (const auto type : UserSelectableTypeSet::All()) {
    if (GetUserSelectableTypeInfo(type).data_type_group.Has(data_type)) {
      CHECK(!selectable_type.has_value())
          << "Data type " << DataTypeToDebugString(data_type)
          << " corresponds to multiple user selectable types.";
      selectable_type = type;
    }
  }

  return selectable_type;
}

#if BUILDFLAG(IS_CHROMEOS)
const char* GetUserSelectableOsTypeName(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).type_name;
}

std::string UserSelectableOsTypeSetToString(UserSelectableOsTypeSet types) {
  std::string result;
  for (UserSelectableOsType type : types) {
    if (!result.empty()) {
      result += ", ";
    }
    result += GetUserSelectableOsTypeName(type);
  }
  return result;
}

std::optional<UserSelectableOsType> GetUserSelectableOsTypeFromString(
    const std::string& type) {
  if (type == kOsAppsTypeName) {
    return UserSelectableOsType::kOsApps;
  }
  if (type == kOsPreferencesTypeName) {
    return UserSelectableOsType::kOsPreferences;
  }
  if (type == kOsWifiConfigurationsTypeName) {
    return UserSelectableOsType::kOsWifiConfigurations;
  }

  // Some pref types migrated from browser prefs to OS prefs. Map the browser
  // type name to the OS type so that enterprise policy SyncTypesListDisabled
  // still applies to the migrated names.
  // TODO(crbug.com/40678410): Rename "osApps" to "apps" and
  // "osWifiConfigurations" to "wifiConfigurations", and remove the mapping for
  // "preferences".
  if (type == kAppsTypeName) {
    return UserSelectableOsType::kOsApps;
  }
  if (type == kWifiConfigurationsTypeName) {
    return UserSelectableOsType::kOsWifiConfigurations;
  }
  if (type == kPreferencesTypeName) {
    return UserSelectableOsType::kOsPreferences;
  }
  return std::nullopt;
}

DataTypeSet UserSelectableOsTypeToAllDataTypes(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).data_type_group;
}

DataType UserSelectableOsTypeToCanonicalDataType(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).canonical_data_type;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::ostream& operator<<(std::ostream& stream, const UserSelectableType& type) {
  return stream << GetUserSelectableTypeName(type);
}

std::ostream& operator<<(std::ostream& stream,
                         const UserSelectableTypeSet& types) {
  return stream << UserSelectableTypeSetToString(types);
}

}  // namespace syncer
