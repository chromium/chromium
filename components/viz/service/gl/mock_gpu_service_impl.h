// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_MOCK_GPU_SERVICE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_GL_MOCK_GPU_SERVICE_IMPL_H_

#include "components/viz/service/gl/gpu_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {

class MockGpuServiceImpl : public GpuServiceImpl {
 public:
  explicit MockGpuServiceImpl(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const std::optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gfx::GpuExtraInfo& gpu_extra_info,
      InitParams init_params);
  MockGpuServiceImpl();
  ~MockGpuServiceImpl() override;

  MOCK_METHOD(void,
              StartPeakMemoryMonitor,
              (uint32_t sequence_num),
              (override));

  MOCK_METHOD(void,
              GetPeakMemoryUsageOnMainThread,
              (uint32_t sequence_num, GetPeakMemoryUsageCallback callback),
              (override));
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_MOCK_GPU_SERVICE_IMPL_H_
