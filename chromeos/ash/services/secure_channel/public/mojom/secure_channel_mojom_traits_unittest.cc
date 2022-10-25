// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_mojom_traits.h"

#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SecureChannelMojomEnumTraitsTest, ConnectionPriority) {
  static constexpr ash::secure_channel::ConnectionPriority
      kTestConnectionPriorities[] = {
          ash::secure_channel::ConnectionPriority::kLow,
          ash::secure_channel::ConnectionPriority::kMedium,
          ash::secure_channel::ConnectionPriority::kHigh};

  for (const auto& priority_in : kTestConnectionPriorities) {
    ash::secure_channel::ConnectionPriority priority_out;

    ash::secure_channel::mojom::ConnectionPriority serialized_priority =
        mojo::EnumTraits<
            ash::secure_channel::mojom::ConnectionPriority,
            ash::secure_channel::ConnectionPriority>::ToMojom(priority_in);
    ASSERT_TRUE(
        (mojo::EnumTraits<ash::secure_channel::mojom::ConnectionPriority,
                          ash::secure_channel::ConnectionPriority>::
             FromMojom(serialized_priority, &priority_out)));
    EXPECT_EQ(priority_in, priority_out);
  }
}
