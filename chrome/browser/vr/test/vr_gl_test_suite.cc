// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_gl_test_suite.h"

#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_image_test_support.h"

#if defined(VR_USE_COMMAND_BUFFER)
#include "gpu/command_buffer/client/gles2_lib.h"  // nogncheck
#include "gpu/config/gpu_info_collector.h"        // nogncheck
#include "gpu/config/gpu_preferences.h"           // nogncheck
#include "gpu/config/gpu_util.h"                  // nogncheck
#include "gpu/ipc/in_process_command_buffer.h"    // nogncheck
#endif  // defined(VR_USE_COMMAND_BUFFER)

namespace vr {

VrGlTestSuite::VrGlTestSuite(int argc, char** argv) : VrTestSuite(argc, argv) {}

void VrGlTestSuite::Initialize() {
  VrTestSuite::Initialize();

  gl::GLImageTestSupport::InitializeGL(base::nullopt);

#if defined(VR_USE_COMMAND_BUFFER)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  gpu::GPUInfo gpu_info;
  gpu::CollectGraphicsInfoForTesting(&gpu_info);
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu::GpuPreferences(), command_line, nullptr);
  // Always enable gpu and oop raster, regardless of platform and blacklist.
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  gpu::InProcessCommandBuffer::InitializeDefaultServiceForTesting(
      gpu_feature_info);
  gles2::Initialize();
#endif  // defined(VR_USE_COMMAND_BUFFER)
}

void VrGlTestSuite::Shutdown() {
  gl::GLImageTestSupport::CleanupGL();
  vr::VrTestSuite::Shutdown();
}

}  // namespace vr
