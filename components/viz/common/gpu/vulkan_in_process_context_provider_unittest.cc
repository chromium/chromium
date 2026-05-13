// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class VulkanInProcessContextProviderTest : public testing::Test {
 public:
  VulkanInProcessContextProviderTest() {
    feature_list_.InitAndEnableFeature(base::kStatefulMemoryPressure);
  }

  void CreateVulkanInProcessContextProvider(
      uint32_t sync_cpu_memory_limit,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15)) {
    context_provider_ = new VulkanInProcessContextProvider(
        nullptr, 0, sync_cpu_memory_limit,
        cooldown_duration_at_memory_pressure_critical);
  }

  void TearDown() override { context_provider_.reset(); }

  void SendMemoryPressureSignal(base::MemoryPressureLevel level) {
    base::RunLoop run_loop;
    base::MemoryPressureListenerRegistry::SimulatePressureNotificationAsync(
        level, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::MemoryPressureListenerRegistry registry_;
  scoped_refptr<VulkanInProcessContextProvider> context_provider_;
};

TEST_F(VulkanInProcessContextProviderTest,
       NotifyMemoryPressureChangesSyncCpuMemoryLimit) {
  const uint32_t kTestSyncCpuMemoryLimit = 1200;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit);

  // Initial state.
  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());

  // Critical pressure -> 0% limit.
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(0u, limit.value());

  // Pressure subsides -> 100% limit.
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_NONE);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());

  // Moderate pressure -> 50% limit.
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit / 2, limit.value());
}

TEST_F(VulkanInProcessContextProviderTest,
       ZeroSyncCpuMemoryLimitDoesNotChange) {
  CreateVulkanInProcessContextProvider(0);

  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());

  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());

  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_MODERATE);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_FALSE(limit.has_value());
}

TEST_F(VulkanInProcessContextProviderTest,
       NotifyMemoryPressureStatelessCooldown) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(base::kStatefulMemoryPressure);

  const uint32_t kTestSyncCpuMemoryLimit = 1234;
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::Seconds(15));

  // Initial state.
  auto limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());

  // Critical pressure -> 0 limit.
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(0u, limit.value());

  // Pressure level subsides, but we are still in the cooldown period.
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_NONE);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(0u, limit.value());

  // Reset the provider with zero cooldown to verify restoration.
  CreateVulkanInProcessContextProvider(kTestSyncCpuMemoryLimit,
                                       base::TimeDelta());
  SendMemoryPressureSignal(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  limit = context_provider_->GetSyncCpuMemoryLimit();
  EXPECT_TRUE(limit.has_value());
  EXPECT_EQ(kTestSyncCpuMemoryLimit, limit.value());
}

}  // namespace viz
