// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(MachineIdProviderNonWinTest, GetId) {
  const bool has_machine_name = !base::SysInfo::HardwareModelName().empty();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
  DCHECK(has_machine_name);
#endif

  // Should only return a machine ID if the hardware model name is available.
  if (has_machine_name) {
    const std::string id1 = MachineIdProvider::GetMachineId();
    EXPECT_TRUE(MachineIdProvider::HasId());
    EXPECT_NE(std::string(), id1);

    const std::string id2 = MachineIdProvider::GetMachineId();
    EXPECT_EQ(id1, id2);
  } else {
    EXPECT_FALSE(MachineIdProvider::HasId());
  }
}

}  // namespace metrics
