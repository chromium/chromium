// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/gpu_metrics_provider.h"

#include <string>

#include "components/metrics/early_metrics_guard.h"
#include "components/metrics/environment_recorder.h"
#include "components/prefs/testing_pref_service.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {
namespace {

class TestGPUMetricsProvider : public GPUMetricsProvider {
 public:
  TestGPUMetricsProvider() = default;

  void SetGPUInfo(const gpu::GPUInfo& gpu_info) { gpu_info_ = gpu_info; }

 protected:
  gpu::GPUInfo GetGPUInfo() const override { return gpu_info_; }

 private:
  gpu::GPUInfo gpu_info_;
};

class GPUMetricsProviderTest : public testing::Test {
 public:
  GPUMetricsProviderTest() {
    EnvironmentRecorder::RegisterPrefs(local_state_.registry());
  }

 protected:
  TestingPrefServiceSimple local_state_;
};

TEST_F(GPUMetricsProviderTest, ProvideSystemProfileMetrics_Initialized) {
  TestGPUMetricsProvider provider;

  gpu::GPUInfo gpu_info;
  gpu_info.active_gpu().vendor_id = 123;
  gpu_info.active_gpu().device_id = 456;
  gpu_info.active_gpu().driver_version = "1.2.3";
  gpu_info.gl_vendor = "gl_vendor_str";
  gpu_info.gl_renderer = "gl_renderer_str";

  provider.SetGPUInfo(gpu_info);

  SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  const auto& gpu = system_profile.hardware().gpu();
  EXPECT_EQ(123u, gpu.vendor_id());
  EXPECT_EQ(456u, gpu.device_id());
  EXPECT_EQ("1.2.3", gpu.driver_version());
  EXPECT_EQ("gl_vendor_str", gpu.gl_vendor());
  EXPECT_EQ("gl_renderer_str", gpu.gl_renderer());
}

TEST_F(GPUMetricsProviderTest, ProvideSystemProfileMetrics_NotInitialized) {
  TestGPUMetricsProvider provider;
  provider.SetGPUInfo(gpu::GPUInfo());

  SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  // Outside early startup, nothing is written until GPU info is available.
  EXPECT_FALSE(system_profile.hardware().has_gpu());
}

TEST_F(GPUMetricsProviderTest,
       ProvideSystemProfileMetrics_UninitializedFallback) {
  // Pre-seed the cache via EnvironmentRecorder.
  SystemProfileProto cached_profile;
  auto* gpu = cached_profile.mutable_hardware()->mutable_gpu();
  gpu->set_vendor_id(789);
  gpu->set_device_id(101);
  gpu->set_driver_version("4.5.6");

  EnvironmentRecorder recorder(&local_state_);
  recorder.SerializeAndRecordEnvironmentToPrefs(cached_profile);

  TestGPUMetricsProvider provider;

  gpu::GPUInfo empty_gpu_info;
  provider.SetGPUInfo(empty_gpu_info);

  EarlyMetricsGuard guard(&local_state_);
  SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  const auto& output_gpu = system_profile.hardware().gpu();
  EXPECT_EQ(789u, output_gpu.vendor_id());
  EXPECT_EQ(101u, output_gpu.device_id());
  EXPECT_EQ("4.5.6", output_gpu.driver_version());
  EXPECT_FALSE(output_gpu.has_gl_vendor());
  EXPECT_FALSE(output_gpu.has_gl_renderer());
}

}  // namespace
}  // namespace metrics
