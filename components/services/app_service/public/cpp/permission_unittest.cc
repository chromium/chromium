// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/permission.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using PermissionTest = testing::Test;

TEST_F(PermissionTest, VerifyPermissionConvert) {
  {
    // Verify the convert function can work for null permission.
    EXPECT_FALSE(ConvertDictToPermission(ConvertPermissionToDict(nullptr)));
  }

  {
    auto permission = std::make_unique<Permission>(
        PermissionType::kCamera, /*PermissionValue=*/true, /*is_managed=*/true,
        "details");
    EXPECT_EQ(*permission,
              *ConvertDictToPermission(ConvertPermissionToDict(permission)));
  }

  {
    auto permission = std::make_unique<Permission>(
        PermissionType::kMicrophone, /*PermissionValue=*/TriState::kAllow,
        /*is_managed=*/false);
    EXPECT_EQ(*permission,
              *ConvertDictToPermission(ConvertPermissionToDict(permission)));
  }
}

TEST_F(PermissionTest, VerifyPermissionsConvert) {
  {
    // Verify the convert function can work for the empty permissions.
    Permissions permissions;
    base::Value::List list = ConvertPermissionsToList(permissions);
    EXPECT_TRUE(IsEqual(permissions, ConvertListToPermissions(&list)));
  }

  {
    Permissions permissions;
    permissions.push_back(std::make_unique<Permission>(
        PermissionType::kLocation, /*PermissionValue=*/false,
        /*is_managed=*/true, "details"));
    permissions.push_back(std::make_unique<Permission>(
        PermissionType::kPrinting, /*PermissionValue=*/TriState::kBlock,
        /*is_managed=*/false));
    base::Value::List list = ConvertPermissionsToList(permissions);
    EXPECT_TRUE(IsEqual(permissions, ConvertListToPermissions(&list)));
  }
}

}  // namespace apps
