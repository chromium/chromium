// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
#define COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_

#include <string>

#include "base/optional.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/enum_set.h"
#include "components/sync/base/model_type.h"

namespace syncer {

enum class UserSelectableType {
  kBookmarks,
  kFirstType = kBookmarks,

  kPreferences,
  kPasswords,
  kAutofill,
  kThemes,
  kHistory,
  kExtensions,
  kApps,
  kReadingList,
  kTabs,
  kWifiConfigurations,
  kLastType = kWifiConfigurations
};

using UserSelectableTypeSet = EnumSet<UserSelectableType,
                                      UserSelectableType::kFirstType,
                                      UserSelectableType::kLastType>;

const char* GetUserSelectableTypeName(UserSelectableType type);
// Returns the type if the string matches a known type.
base::Optional<UserSelectableType> GetUserSelectableTypeFromString(
    const std::string& type);
std::string UserSelectableTypeSetToString(UserSelectableTypeSet types);
ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type);

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS provides a separate UI with sync controls for OS data types. Note
// that wallpaper is a special case due to its reliance on apps, so while it
// appears in the UI, it is not included in this enum.
// TODO(https://crbug.com/967987): Break this dependency.
enum class UserSelectableOsType {
  kOsApps,
  kFirstType = kOsApps,

  kOsPreferences,
  kOsWifiConfigurations,
  kLastType = kOsWifiConfigurations
};

using UserSelectableOsTypeSet = EnumSet<UserSelectableOsType,
                                        UserSelectableOsType::kFirstType,
                                        UserSelectableOsType::kLastType>;

const char* GetUserSelectableOsTypeName(UserSelectableOsType type);
ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type);
ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type);

// Returns the type if the string matches a known OS type.
base::Optional<UserSelectableOsType> GetUserSelectableOsTypeFromString(
    const std::string& type);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
