// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_GPU_UTIL_HEADLESS_GPU_UTIL_H_
#define COMPONENTS_HEADLESS_GPU_UTIL_HEADLESS_GPU_UTIL_H_

#include "base/command_line.h"

namespace headless {

// Enables SwiftShader for WebGL if --enable-unsafe-swiftshader is passed, or
// disables GPU otherwise.
void ConfigureHeadlessSwiftShaderCommandLine(base::CommandLine& command_line);

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_GPU_UTIL_HEADLESS_GPU_UTIL_H_
