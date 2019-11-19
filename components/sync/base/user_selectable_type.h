// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
#define COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_

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
  kWifiConfigurations,
  kTabs,
  kLastType = kTabs
};

using UserSelectableTypeSet = EnumSet<UserSelectableType,
                                      UserSelectableType::kFirstType,
                                      UserSelectableType::kLastType>;

const char* GetUserSelectableTypeName(UserSelectableType type);
ModelTypeSet UserSelectableTypeToAllModelTypes(UserSelectableType type);

ModelType UserSelectableTypeToCanonicalModelType(UserSelectableType type);
int UserSelectableTypeToHistogramInt(UserSelectableType type);

constexpr int UserSelectableTypeHistogramNumEntries() {
  return static_cast<int>(ModelType::NUM_ENTRIES);
}

#if defined(OS_CHROMEOS)
// Chrome OS provides a separate UI with sync controls for OS data types.
enum class UserSelectableOsType {
  kOsPreferences,
  kFirstType = kOsPreferences,

  kPrinters,
  kLastType = kPrinters
};

using UserSelectableOsTypeSet = EnumSet<UserSelectableOsType,
                                        UserSelectableOsType::kFirstType,
                                        UserSelectableOsType::kLastType>;

const char* GetUserSelectableOsTypeName(UserSelectableOsType type);
ModelTypeSet UserSelectableOsTypeToAllModelTypes(UserSelectableOsType type);
ModelType UserSelectableOsTypeToCanonicalModelType(UserSelectableOsType type);
#endif  // defined(OS_CHROMEOS)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_USER_SELECTABLE_TYPE_H_
