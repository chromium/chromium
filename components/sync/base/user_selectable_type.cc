// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <ostream>

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/model_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const ModelType canonical_model_type;
  const ModelTypeSet model_type_group;
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

UserSelectableTypeInfo GetUserSelectableTypeInfo(UserSelectableType type) {
  static_assert(47 == syncer::GetNumModelTypes(),
                "Almost always when adding a new ModelType, you must tie it to "
                "a UserSelectableType below (new or existing) so the user can "
                "disable syncing of that data. Today you must also update the "
                "UI code yourself; crbug.com/1067282 and related bugs will "
                "improve that");
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {kBookmarksTypeName, BOOKMARKS, {BOOKMARKS, POWER_BOOKMARK}};
    case UserSelectableType::kPreferences:
      // TODO(crbug.com/1369259): Add GetPreconditionState() logic to check
      // history state as a precondition for SEGMENTATION.
      return {kPreferencesTypeName,
              PREFERENCES,
              {PREFERENCES, DICTIONARY, PRIORITY_PREFERENCES, SEARCH_ENGINES,
               SEGMENTATION}};
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // In Ash, "Apps" part of Chrome OS settings.
      return {kAppsTypeName, UNSPECIFIED};
#else
      return {kAppsTypeName, APPS, {APPS, APP_SETTINGS, WEB_APPS}};
#endif
    case UserSelectableType::kReadingList:
      return {kReadingListTypeName, READING_LIST, {READING_LIST}};
    case UserSelectableType::kTabs:
      return {kTabsTypeName, SESSIONS, {SESSIONS}};
    case UserSelectableType::kSavedTabGroups:
      return {kSavedTabGroupsTypeName, SAVED_TAB_GROUP, {SAVED_TAB_GROUP}};
    case UserSelectableType::kPayments:
      return {kPaymentsTypeName,
              AUTOFILL_WALLET_DATA,
              {AUTOFILL_WALLET_CREDENTIAL, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA, AUTOFILL_WALLET_OFFER,
               AUTOFILL_WALLET_USAGE}};
  }
  NOTREACHED();
  return {nullptr, UNSPECIFIED, {}};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).type_name;
}

absl::optional<UserSelectableType> GetUserSelectableTypeFromString(
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
  return absl::nullopt;
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

ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).model_type_group;
}

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_model_type;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

absl::optional<UserSelectableOsType> GetUserSelectableOsTypeFromString(
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
  // TODO(https://crbug.com/1059309): Rename "osApps" to "apps" and
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
  return absl::nullopt;
}

ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).model_type_group;
}

ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).canonical_model_type;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::ostream& operator<<(std::ostream& stream, const UserSelectableType& type) {
  return stream << GetUserSelectableTypeName(type);
}

std::ostream& operator<<(std::ostream& stream,
                         const UserSelectableTypeSet& types) {
  return stream << UserSelectableTypeSetToString(types);
}

}  // namespace syncer
