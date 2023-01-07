// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class VulkanInProcessContextProviderTest : public testing::Test {
 public:
  void CreateVulkanInProcessContextProvider(
      uint32_t sync_cpu_memory_limit,
      const base::TimeDelta& cooldown_duration_at_memory_pressure_critical) {
    context_provider_ = new VulkanInProcessContextProvider(
        nullptr, 0, sync_cpu_memory_limit,
        cooldown_duration_at_memory_pressure_critical);
  }

  void TearDown() override { context_provider_.reset(); }

  void SendCriticalMemoryPressureSignal() {
    context_provider_->OnMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }

 protected:
  scoped_refptr<VulkanInProcessContextProvider> context_provider_;
};

TEST_F(VulkanInProcessContextProviderTest,
       NotifyMemoryPressureChangesSyncCpuMemoryLimit) {
  const uint32_t kTestSyncCpuMemoryLimit = 1234;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::TimeDelta::Max());

  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());

  SendCriticalMemoryPressureSignal();
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(0u, limit.value());
}

TEST_F(VulkanInProcessContextProviderTest,
       ZeroSyncCpuMemoryLimitDoesNotChange) {
  CreateVulkanInProcessContextProvider(0, base::TimeDelta::Max());

  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());

  SendCriticalMemoryPressureSignal();
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());
}

TEST_F(VulkanInProcessContextProviderTest, SyncCpuMemoryResetsAfterCooldown) {
  const uint32_t kTestSyncCpuMemoryLimit = 1234;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::TimeDelta::Min());

  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());

  SendCriticalMemoryPressureSignal();
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());
}

}  // namespace viz
