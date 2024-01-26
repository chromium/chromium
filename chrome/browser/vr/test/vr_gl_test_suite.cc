// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_gl_test_suite.h"

#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_test_support.h"

#if defined(VR_USE_COMMAND_BUFFER)
#include "gpu/command_buffer/client/gles2_lib.h"  // nogncheck
#include "gpu/ipc/test_gpu_thread_holder.h"       // nogncheck
#endif  // defined(VR_USE_COMMAND_BUFFER)

namespace vr {

VrGlTestSuite::VrGlTestSuite(int argc, char** argv) : VrTestSuite(argc, argv) {}

void VrGlTestSuite::Initialize() {
  VrTestSuite::Initialize();

  display_ = gl::GLTestSupport::InitializeGL(std::nullopt);

#if defined(VR_USE_COMMAND_BUFFER)
  // Always enable gpu and oop raster, regardless of platform and denylist.
  auto* gpu_feature_info = gpu::GetTestGpuThreadHolder()->GetGpuFeatureInfo();
  gpu_feature_info
      ->status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  gles2::Initialize();
#endif  // defined(VR_USE_COMMAND_BUFFER)
}

void VrGlTestSuite::Shutdown() {
  gl::GLTestSupport::CleanupGL(display_);
  vr::VrTestSuite::Shutdown();
}

}  // namespace vr
