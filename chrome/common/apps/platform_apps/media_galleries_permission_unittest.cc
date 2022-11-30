// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests make sure MediaGalleriesPermission values are parsed correctly.

#include <memory>

#include "base/values.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission_data.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::APIPermissionID;

namespace chrome_apps {

namespace {

void CheckFromValue(extensions::APIPermission* permission,
                    base::Value* value,
                    bool success_expected) {
  std::string error;
  std::vector<std::string> unhandled;
  EXPECT_EQ(success_expected, permission->FromValue(value, &error, &unhandled));
  EXPECT_EQ(success_expected, error.empty());
  EXPECT_TRUE(unhandled.empty());
}

TEST(MediaGalleriesPermissionTest, GoodValues) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // access_type + all_detected
  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), &value, true);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), &value, true);

  // all_detected
  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  CheckFromValue(permission.get(), &value, true);

  // access_type
  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), &value, true);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), &value, true);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), &value, true);

  // Repeats do not make a difference.
  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  CheckFromValue(permission.get(), &value, true);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), &value, true);
}

TEST(MediaGalleriesPermissionTest, BadValues) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // copyTo and delete without read
  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  CheckFromValue(permission.get(), &value, false);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), &value, false);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), &value, false);

  // copyTo without delete
  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), &value, false);

  // Repeats do not make a difference.
  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  CheckFromValue(permission.get(), &value, false);

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), &value, false);
}

TEST(MediaGalleriesPermissionTest, UnknownValues) {
  std::string error;
  std::vector<std::string> unhandled;
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // A good one and an unknown one.
  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append("Unknown");
  EXPECT_TRUE(permission->FromValue(&value, &error, &unhandled));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(1U, unhandled.size());
  error.clear();
  unhandled.clear();

  // Multiple unknown permissions.
  value_list.clear();
  value_list.Append("Unknown1");
  value_list.Append("Unknown2");
  EXPECT_TRUE(permission->FromValue(&value, &error, &unhandled));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(2U, unhandled.size());
  error.clear();
  unhandled.clear();

  // Unnknown with a nullptr argument.
  value_list.clear();
  value_list.Append("Unknown1");
  EXPECT_FALSE(permission->FromValue(&value, &error, nullptr));
  EXPECT_FALSE(error.empty());
  error.clear();
}

TEST(MediaGalleriesPermissionTest, Equal) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  ASSERT_TRUE(permission2->FromValue(&value, nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  ASSERT_TRUE(permission2->FromValue(&value, nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission2->FromValue(&value, nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission2->FromValue(&value, nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));
}

TEST(MediaGalleriesPermissionTest, NotEqual) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  ASSERT_TRUE(permission2->FromValue(&value, nullptr, nullptr));
  EXPECT_FALSE(permission1->Equal(permission2.get()));
}

TEST(MediaGalleriesPermissionTest, ToFromValue) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          APIPermissionID::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  base::Value value(base::Value::Type::LIST);
  base::Value::List& value_list = value.GetList();
  value_list.Append(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  std::unique_ptr<base::Value> vtmp(permission1->ToValue());
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  value_list.Append(MediaGalleriesPermission::kCopyToPermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  value_list.Append(MediaGalleriesPermission::kReadPermission);
  value_list.Append(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(&value, nullptr, nullptr));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value_list.clear();
  // without sub-permission
  ASSERT_TRUE(permission1->FromValue(nullptr, nullptr, nullptr));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), nullptr, nullptr));
  EXPECT_TRUE(permission1->Equal(permission2.get()));
}

}  // namespace

}  // namespace chrome_apps
