// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_TEST_SHADER_H_
#define COMPONENTS_METAL_UTIL_TEST_SHADER_H_

#include "base/callback.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/metal_util/metal_util_export.h"

namespace metal {

enum class METAL_UTIL_EXPORT TestShaderResult {
  // Not attempted (e.g, because macOS version does not support Metal).
  kNotAttempted,
  // Shader compile succeeded.
  kSucceeded,
  // Shader compile failed.
  kFailed,
  // Shader compile timed out.
  kTimedOut,
};

using TestShaderCallback = base::OnceCallback<void(TestShaderResult result)>;

// Attempt to asynchronously compile a trivial Metal shader. Call |callback|
// with the result when the shader succeeds or after |timeout| has elapsed.
// This is used to determine of the Metal shader compiler is resposive. Note
// that |callback| will be called either on another thread or inside the
// TestShader function call. The |seed| parameter is incorporated into the
// source of the shader (to defeat caching).
// https://crbug.com/974219
void METAL_UTIL_EXPORT TestShader(float seed,
                                  TestShaderCallback callback,
                                  const base::TimeDelta& timeout);

// Values for |seed| for the three uses of the TestSahder function. The exact
// values don't matter, only that they are distinct.
constexpr float kTestShaderSeedBrowserTimer = 0.9f;
constexpr float kTestShaderSeedGpuTimer = 1.1f;
constexpr float kTestShaderSeedContextProvider = 2.1f;

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_TEST_SHADER_H_
