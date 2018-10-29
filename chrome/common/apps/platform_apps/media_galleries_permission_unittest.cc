// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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

using content::SocketPermissionRequest;
using extensions::SocketPermissionData;

namespace chrome_apps {

namespace {

void CheckFromValue(extensions::APIPermission* permission,
                    base::ListValue* value,
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
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // access_type + all_detected
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), value.get(), true);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), value.get(), true);

  // all_detected
  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  CheckFromValue(permission.get(), value.get(), true);

  // access_type
  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), value.get(), true);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), value.get(), true);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), value.get(), true);

  // Repeats do not make a difference.
  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  CheckFromValue(permission.get(), value.get(), true);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), value.get(), true);
}

TEST(MediaGalleriesPermissionTest, BadValues) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // copyTo and delete without read
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  CheckFromValue(permission.get(), value.get(), false);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), value.get(), false);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), value.get(), false);

  // copyTo without delete
  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  CheckFromValue(permission.get(), value.get(), false);

  // Repeats do not make a difference.
  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  CheckFromValue(permission.get(), value.get(), false);

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  CheckFromValue(permission.get(), value.get(), false);
}

TEST(MediaGalleriesPermissionTest, UnknownValues) {
  std::string error;
  std::vector<std::string> unhandled;
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission(
      permission_info->CreateAPIPermission());

  // A good one and an unknown one.
  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString("Unknown");
  EXPECT_TRUE(permission->FromValue(value.get(), &error, &unhandled));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(1U, unhandled.size());
  error.clear();
  unhandled.clear();

  // Multiple unknown permissions.
  value.reset(new base::ListValue());
  value->AppendString("Unknown1");
  value->AppendString("Unknown2");
  EXPECT_TRUE(permission->FromValue(value.get(), &error, &unhandled));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(2U, unhandled.size());
  error.clear();
  unhandled.clear();

  // Unnknown with a NULL argument.
  value.reset(new base::ListValue());
  value->AppendString("Unknown1");
  EXPECT_FALSE(permission->FromValue(value.get(), &error, NULL));
  EXPECT_FALSE(error.empty());
  error.clear();
}

TEST(MediaGalleriesPermissionTest, Equal) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  ASSERT_TRUE(permission2->FromValue(value.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  ASSERT_TRUE(permission2->FromValue(value.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission2->FromValue(value.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission2->FromValue(value.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));
}

TEST(MediaGalleriesPermissionTest, NotEqual) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  ASSERT_TRUE(permission2->FromValue(value.get(), NULL, NULL));
  EXPECT_FALSE(permission1->Equal(permission2.get()));
}

TEST(MediaGalleriesPermissionTest, ToFromValue) {
  const extensions::APIPermissionInfo* permission_info =
      extensions::PermissionsInfo::GetInstance()->GetByID(
          extensions::APIPermission::kMediaGalleries);

  std::unique_ptr<extensions::APIPermission> permission1(
      permission_info->CreateAPIPermission());
  std::unique_ptr<extensions::APIPermission> permission2(
      permission_info->CreateAPIPermission());

  std::unique_ptr<base::ListValue> value(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kAllAutoDetectedPermission);
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  std::unique_ptr<base::Value> vtmp(permission1->ToValue());
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  value->AppendString(MediaGalleriesPermission::kCopyToPermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  value->AppendString(MediaGalleriesPermission::kReadPermission);
  value->AppendString(MediaGalleriesPermission::kDeletePermission);
  ASSERT_TRUE(permission1->FromValue(value.get(), NULL, NULL));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  value.reset(new base::ListValue());
  // without sub-permission
  ASSERT_TRUE(permission1->FromValue(NULL, NULL, NULL));

  vtmp = permission1->ToValue();
  ASSERT_TRUE(vtmp);
  ASSERT_TRUE(permission2->FromValue(vtmp.get(), NULL, NULL));
  EXPECT_TRUE(permission1->Equal(permission2.get()));
}

}  // namespace

}  // namespace chrome_apps
