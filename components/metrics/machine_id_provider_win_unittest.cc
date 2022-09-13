// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(MachineIdProviderWinTest, GetId) {
  EXPECT_TRUE(MachineIdProvider::HasId());

  const std::string id1 = MachineIdProvider::GetMachineId();
  EXPECT_NE(std::string(), id1);

  const std::string id2 = MachineIdProvider::GetMachineId();
  EXPECT_EQ(id1, id2);
}

}  // namespace metrics
