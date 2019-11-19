// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include <type_traits>

#include "base/logging.h"
#include "components/sync/base/model_type.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace syncer {

namespace {

struct UserSelectableTypeInfo {
  const char* const type_name;
  const ModelType canonical_model_type;
  const ModelTypeSet model_type_group;
};

UserSelectableTypeInfo GetUserSelectableTypeInfo(UserSelectableType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return {"bookmarks", BOOKMARKS, {BOOKMARKS}};
    case UserSelectableType::kPreferences: {
      ModelTypeSet model_types = {PREFERENCES, DICTIONARY, PRIORITY_PREFERENCES,
                                  SEARCH_ENGINES};
#if defined(OS_CHROMEOS)
      // SplitSettingsSync makes Printers a separate OS setting.
      if (!chromeos::features::IsSplitSettingsSyncEnabled())
        model_types.Put(PRINTERS);
#endif
      return {"preferences", PREFERENCES, model_types};
    }
    case UserSelectableType::kPasswords:
      return {"passwords", PASSWORDS, {PASSWORDS}};
    case UserSelectableType::kAutofill:
      return {"autofill",
              AUTOFILL,
              {AUTOFILL, AUTOFILL_PROFILE, AUTOFILL_WALLET_DATA,
               AUTOFILL_WALLET_METADATA}};
    case UserSelectableType::kThemes:
      return {"themes", THEMES, {THEMES}};
    case UserSelectableType::kHistory:
      return {"typedUrls",
              TYPED_URLS,
              {TYPED_URLS, HISTORY_DELETE_DIRECTIVES, SESSIONS, FAVICON_IMAGES,
               FAVICON_TRACKING, USER_EVENTS}};
    case UserSelectableType::kExtensions:
      return {"extensions", EXTENSIONS, {EXTENSIONS, EXTENSION_SETTINGS}};
    case UserSelectableType::kApps:
      return {
          "apps", APPS, {APPS, APP_SETTINGS, APP_LIST, ARC_PACKAGE, WEB_APPS}};
    case UserSelectableType::kReadingList:
      return {"readingList", READING_LIST, {READING_LIST}};
    case UserSelectableType::kTabs:
      return {"tabs",
              PROXY_TABS,
              {PROXY_TABS, SESSIONS, FAVICON_IMAGES, FAVICON_TRACKING,
               SEND_TAB_TO_SELF}};
    case UserSelectableType::kWifiConfigurations:
      return {"wifiConfigurations", WIFI_CONFIGURATIONS, {WIFI_CONFIGURATIONS}};
  }
  NOTREACHED();
  return {nullptr, UNSPECIFIED};
}

#if defined(OS_CHROMEOS)
UserSelectableTypeInfo GetUserSelectableOsTypeInfo(UserSelectableOsType type) {
  // UserSelectableTypeInfo::type_name is used in js code and shouldn't be
  // changed without updating js part.
  switch (type) {
    case UserSelectableOsType::kOsPreferences:
      return {"osPreferences",
              OS_PREFERENCES,
              {OS_PREFERENCES, OS_PRIORITY_PREFERENCES}};
    case UserSelectableOsType::kPrinters:
      return {"printers", PRINTERS, {PRINTERS}};
  }
}
#endif

}  // namespace

const char* GetUserSelectableTypeName(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).type_name;
}

ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).model_type_group;
}

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type) {
  return GetUserSelectableTypeInfo(type).canonical_model_type;
}

int UserSelectableTypeToHistogramInt(UserSelectableType type) {
  // TODO(crbug.com/1007293): Use ModelTypeHistogramValue instead of casting to
  // int.
  return static_cast<int>(
      ModelTypeHistogramValue(UserSelectableTypeToCanonicalModelType(type)));
}

#if defined(OS_CHROMEOS)
const char* GetUserSelectableOsTypeName(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).type_name;
}

ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).model_type_group;
}

ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type) {
  return GetUserSelectableOsTypeInfo(type).canonical_model_type;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace syncer
