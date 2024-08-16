// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
#define COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_

#include <iosfwd>
#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/data_type.h"

namespace syncer {

// TODO(crbug.com/40210838): once it's impossible to launch Ash-browser only
// UserSelectableOsType will be relevant for Ash, guard UserSelectableType with
// #if !BUILDFLAG(IS_CHROMEOS_ASH) and remove lower level Ash-specific code.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
//
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
  kSavedTabGroups,
  kPayments,
  kSharedTabGroupData,
  kProductComparison,
  kCookies,
  kLastType = kCookies
};

using UserSelectableTypeSet = base::EnumSet<UserSelectableType,
                                            UserSelectableType::kFirstType,
                                            UserSelectableType::kLastType>;

const char* GetUserSelectableTypeName(UserSelectableType type);
// Returns the type if the string matches a known type.
std::optional<UserSelectableType> GetUserSelectableTypeFromString(
    const std::string& type);
std::string UserSelectableTypeSetToString(UserSelectableTypeSet types);
DataTypeSet UserSelectableTypeToAllDataTypes(UserSelectableType type);

DataType UserSelectableTypeToCanonicalDataType(UserSelectableType type);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS provides a separate UI with sync controls for OS data types. Note
// that wallpaper is a special case due to its reliance on apps, so while it
// appears in the UI, it is not included in this enum.
// TODO(crbug.com/40629581): Break this dependency.
enum class UserSelectableOsType {
  kOsApps,
  kFirstType = kOsApps,

  kOsPreferences,
  kOsWifiConfigurations,
  kLastType = kOsWifiConfigurations
};

using UserSelectableOsTypeSet = base::EnumSet<UserSelectableOsType,
                                              UserSelectableOsType::kFirstType,
                                              UserSelectableOsType::kLastType>;

const char* GetUserSelectableOsTypeName(UserSelectableOsType type);
std::string UserSelectableOsTypeSetToString(UserSelectableOsTypeSet types);
DataTypeSet UserSelectableOsTypeToAllDataTypes(UserSelectableOsType type);
DataType UserSelectableOsTypeToCanonicalDataType(UserSelectableOsType type);

// Returns the type if the string matches a known OS type.
std::optional<UserSelectableOsType> GetUserSelectableOsTypeFromString(
    const std::string& type);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// For GTest.
std::ostream& operator<<(std::ostream& stream, const UserSelectableType& type);
std::ostream& operator<<(std::ostream& stream,
                         const UserSelectableTypeSet& types);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
