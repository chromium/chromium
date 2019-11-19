// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/gpu/gpu_metrics_provider.h"

#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

GPUMetricsProvider::GPUMetricsProvider() {
}

GPUMetricsProvider::~GPUMetricsProvider() {
}

void GPUMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  SystemProfileProto::Hardware* hardware =
      system_profile_proto->mutable_hardware();

  const gpu::GPUInfo& gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  SystemProfileProto::Hardware::Graphics* gpu =
      hardware->mutable_gpu();
  gpu->set_vendor_id(active_gpu.vendor_id);
  gpu->set_device_id(active_gpu.device_id);
  gpu->set_driver_version(active_gpu.driver_version);
  gpu->set_gl_vendor(gpu_info.gl_vendor);
  gpu->set_gl_renderer(gpu_info.gl_renderer);
}

}  // namespace metrics
