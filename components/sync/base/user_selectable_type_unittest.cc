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
  // Data types which are only selectable on ChromeOS where they are mapped to
  // `UserSelectableOsType`.
  DataTypeSet ChromeOsOnlyTypes() {
    DataTypeSet data_types;

    data_types.Put(APP_LIST);
    data_types.Put(ARC_PACKAGE);
    data_types.Put(OS_PREFERENCES);
    data_types.Put(OS_PRIORITY_PREFERENCES);
    data_types.Put(PRINTERS);
    data_types.Put(PRINTERS_AUTHORIZATION_SERVERS);
    data_types.Put(WIFI_CONFIGURATIONS);

    return data_types;
  }

  // Data types which are mapped to `UserSelectableOsType` on ChromeOS, but are
  // mapped to `UserSelectableType` on other platforms.
  DataTypeSet ChromeOsSpecificTypes() {
    DataTypeSet data_types;

    data_types.Put(APPS);
    data_types.Put(APP_SETTINGS);
    data_types.Put(WEB_APPS);
    data_types.Put(WEB_APKS);

    return data_types;
  }

  // Data types with a different `UserSelectableType` mapping across platforms.
  DataTypeSet AmbiguousTypes() {
    DataTypeSet data_types;

    data_types.Put(SAVED_TAB_GROUP);
    data_types.Put(SHARED_TAB_GROUP_DATA);
    data_types.Put(COLLABORATION_GROUP);
    data_types.Put(SHARED_TAB_GROUP_ACCOUNT_DATA);
    data_types.Put(SHARED_COMMENT);

    // TODO(crbug.com/445841720): In CL #3, map AI_THREAD to an existing
    // selectable type or to a new one and remove it from here (unless it's
    // ambiguous).
    data_types.Put(AI_THREAD);
    // TODO(crbug.com/445840788): In CL #3, map CONTEXTUAL_TASK to an existing
    // selectable type or to a new one and remove it from here (unless it's
    // ambiguous).
    data_types.Put(CONTEXTUAL_TASK);

    return data_types;
  }
};

TEST_F(UserSelectableTypeTest, GetUserSelectableTypeFromDataType) {
  // These data types do not have a corresponding `UserSelectableType` in
  // `GetUserSelectableTypeInfo()` and will therefore return `std::nullopt`.
  DataTypeSet non_convertible_types =
      base::Union(base::Union(AlwaysPreferredUserTypes(), ControlTypes()),
                  ChromeOsOnlyTypes());

  for (const auto type : DataTypeSet::All()) {
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

TEST_F(UserSelectableTypeTest, UserSelectableTypeSetToValueList) {
  UserSelectableTypeSet types = {UserSelectableType::kBookmarks,
                                 UserSelectableType::kPasswords,
                                 UserSelectableType::kPreferences};
  base::Value::List value_list = UserSelectableTypeSetToValueList(types);
  EXPECT_EQ(value_list, base::Value::List()
                            .Append("bookmarks")
                            .Append("preferences")
                            .Append("passwords"));
}

TEST_F(UserSelectableTypeTest, ValueListToUserSelectableTypeSet) {
  base::Value::List value_list = base::Value::List()
                                     .Append("bookmarks")
                                     .Append("passwords")
                                     .Append("preferences");
  UserSelectableTypeSet types = ValueListToUserSelectableTypeSet(value_list);
  EXPECT_EQ(types, UserSelectableTypeSet({UserSelectableType::kBookmarks,
                                          UserSelectableType::kPreferences,
                                          UserSelectableType::kPasswords}));
}

}  // namespace

}  // namespace syncer
