// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/cpu_probe_win.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "components/system_cpu/cpu_probe.h"
#include "components/system_cpu/pressure_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_cpu {

class CpuProbeWinTest : public testing::Test {
 public:
  CpuProbeWinTest() = default;

  ~CpuProbeWinTest() override = default;

  void SetUp() override {
    probe_ = std::make_unique<FakePlatformCpuProbe<CpuProbeWin>>();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakePlatformCpuProbe<CpuProbeWin>> probe_;
};

TEST_F(CpuProbeWinTest, ProductionDataNoCrash) {
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_GE(sample->cpu_utilization, 0.0);
  EXPECT_LE(sample->cpu_utilization, 1.0);
}

}  // namespace system_cpu
