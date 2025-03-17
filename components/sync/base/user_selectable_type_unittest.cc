// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_selectable_type.h"

#include "base/containers/enum_set.h"
#include "components/sync/base/data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class UserSelectableTypeTest : public ::testing::Test {
 public:
  DataTypeSet OsOnlyTypes() {
    DataTypeSet data_types;

    data_types.Put(APP_LIST);
    data_types.Put(ARC_PACKAGE);
    data_types.Put(OS_PREFERENCES);
    data_types.Put(OS_PRIORITY_PREFERENCES);
    data_types.Put(PRINTERS);
    data_types.Put(PRINTERS_AUTHORIZATION_SERVERS);
    data_types.Put(WORKSPACE_DESK);
    data_types.Put(WIFI_CONFIGURATIONS);

    return data_types;
  }

  DataTypeSet ChromeOsSpecificTypes() {
    DataTypeSet data_types;

    data_types.Put(APPS);
    data_types.Put(APP_SETTINGS);
    data_types.Put(WEB_APPS);
    data_types.Put(WEB_APKS);

    return data_types;
  }

  DataTypeSet AmbiguousTypes() {
    DataTypeSet data_types;

    data_types.Put(SAVED_TAB_GROUP);
    data_types.Put(SHARED_TAB_GROUP_DATA);
    data_types.Put(COLLABORATION_GROUP);
    data_types.Put(SHARED_TAB_GROUP_ACCOUNT_DATA);

    return data_types;
  }
};

TEST_F(UserSelectableTypeTest, GetUserSelectableTypeFromDataType) {
  // These data types do not have a corresponding `UserSelectableType` in
  // `GetUserSelectableTypeInfo()` and will therefore return `std::nullopt`.
  DataTypeSet non_convertible_types = base::Union(
      base::Union(AlwaysPreferredUserTypes(), ControlTypes()), OsOnlyTypes());

  for (const auto type : DataTypeSet::All()) {
    // TODO (crbug.com/361625648): Removing
    // `UserSelectableType::kSharedTabGroupData` would get rid of ambiguity in
    // the conversion from data types to user selectable types on most
    // platforms. However, this check would then still be required for Android
    // and iOS because of `UserSelectableType::kTabs`.
    if (AmbiguousTypes().Has(type)) {
      continue;
    }

#if BUILDFLAG(IS_CHROMEOS)
    if (ChromeOsSpecificTypes().Has(type)) {
      EXPECT_FALSE(GetUserSelectableTypeFromDataType(type).has_value())
          << "Failed for data type: " << type;
      continue;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    if (non_convertible_types.Has(type)) {
      EXPECT_FALSE(GetUserSelectableTypeFromDataType(type).has_value())
          << "Failed for data type: " << type;
    } else {
      EXPECT_TRUE(GetUserSelectableTypeFromDataType(type).has_value())
          << "Failed for data type: " << type;
    }
  }
}

}  // namespace

}  // namespace syncer
