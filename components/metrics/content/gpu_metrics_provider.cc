// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/gpu_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

GPUMetricsProvider::GPUMetricsProvider() = default;

GPUMetricsProvider::~GPUMetricsProvider() = default;

void GPUMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();

  const gpu::GPUInfo& gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  SystemProfileProto::Hardware::Graphics* gpu = hardware->mutable_gpu();
  gpu->set_vendor_id(active_gpu.vendor_id);
  gpu->set_device_id(active_gpu.device_id);
  gpu->set_driver_version(active_gpu.driver_version);
  gpu->set_gl_vendor(gpu_info.gl_vendor);
  gpu->set_gl_renderer(gpu_info.gl_renderer);
}

void GPUMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
#if BUILDFLAG(IS_ANDROID)
  const gpu::GPUInfo& gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();

  // It's intentional that GPU.HardwareSupportsVulkan isn't logged for the
  // previous session. The value is unlikely to change between sessions but
  // previous session data is recorded before GPU service is launched and
  // current session value is known.
  if (gpu_info.IsInitialized()) {
    UMA_HISTOGRAM_BOOLEAN("GPU.HardwareSupportsVulkan",
                          gpu_info.hardware_supports_vulkan);
  }
#endif
}

}  // namespace metrics
