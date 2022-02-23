// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_type_mojom_traits.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that serialization and deserialization works with updating readiness.
TEST(AppTypeMojomTraitsTest, RoundTripReadiness) {
  static constexpr apps::Readiness kTestReadiness[] = {
      apps::Readiness::kUnknown,
      apps::Readiness::kReady,
      apps::Readiness::kDisabledByBlocklist,
      apps::Readiness::kDisabledByPolicy,
      apps::Readiness::kDisabledByUser,
      apps::Readiness::kTerminated,
      apps::Readiness::kUninstalledByUser,
      apps::Readiness::kRemoved,
      apps::Readiness::kUninstalledByMigration};

  for (auto readiness_in : kTestReadiness) {
    apps::Readiness readiness_out;

    chromeos::settings::app_notification::mojom::Readiness
        serialized_readiness = mojo::EnumTraits<
            chromeos::settings::app_notification::mojom::Readiness,
            apps::Readiness>::ToMojom(readiness_in);
    ASSERT_TRUE((
        mojo::EnumTraits<chromeos::settings::app_notification::mojom::Readiness,
                         apps::Readiness>::FromMojom(serialized_readiness,
                                                     &readiness_out)));
    EXPECT_EQ(readiness_in, readiness_out);
  }
}

TEST(AppTypeMojomTraitsTest, RoundTripPermissions) {
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kUnknown,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kCamera,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kMicrophone,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kNotifications,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAsk),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kContacts,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kStorage,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kPrinting,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                chromeos::settings::app_notification::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
}
