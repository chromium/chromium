// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_type_mojom_traits.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_notification_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
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
      apps::Readiness::kUninstalledByNonUser,
      apps::Readiness::kDisabledByLocalSettings};

  for (auto readiness_in : kTestReadiness) {
    apps::Readiness readiness_out;

    ash::settings::app_notification::mojom::Readiness serialized_readiness =
        mojo::EnumTraits<ash::settings::app_notification::mojom::Readiness,
                         apps::Readiness>::ToMojom(readiness_in);
    ASSERT_TRUE(
        (mojo::EnumTraits<ash::settings::app_notification::mojom::Readiness,
                          apps::Readiness>::FromMojom(serialized_readiness,
                                                      &readiness_out)));
    EXPECT_EQ(readiness_in, readiness_out);
  }
}
