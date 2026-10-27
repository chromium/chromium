// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/gpu_util/headless_gpu_util.h"

#include "content/public/common/content_switches.h"
#include "ui/gl/gl_switches.h"

namespace headless {

void ConfigureHeadlessSwiftShaderCommandLine(base::CommandLine& command_line) {
  if (command_line.HasSwitch(::switches::kEnableUnsafeSwiftShader)) {
    command_line.AppendSwitchASCII(::switches::kUseGL,
                                   gl::kGLImplementationANGLEName);
    command_line.AppendSwitchASCII(
        ::switches::kUseANGLE, gl::kANGLEImplementationSwiftShaderForWebGLName);
  } else if (!command_line.HasSwitch(::switches::kDisableGpu)) {
    command_line.AppendSwitch(::switches::kDisableGpu);
  }
}

}  // namespace headless
