// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/mock_gpu_service_impl.h"

#include <utility>

namespace viz {

MockGpuServiceImpl::MockGpuServiceImpl(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info,
    InitParams init_params)
    : GpuServiceImpl(gpu_preferences,
                     gpu_info,
                     gpu_feature_info,
                     gpu_info_for_hardware_gpu,
                     gpu_feature_info_for_hardware_gpu,
                     gpu_extra_info,
                     std::move(init_params)) {}

MockGpuServiceImpl::MockGpuServiceImpl() = default;

MockGpuServiceImpl::~MockGpuServiceImpl() = default;
}  // namespace viz
