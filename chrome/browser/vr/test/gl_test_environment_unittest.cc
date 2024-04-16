// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/gl_test_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace vr {

// TODO(crbug.com/40881517): Re-enable this test on MSAN if not removed.
// TODO(crbug.com/40190919): Re-enable this test on Linux in general, or fully
// remove if DrawVrBrowsingMode is removed (see
// https://chromium-review.googlesource.com/c/chromium/src/+/4102520/comments/b1cb2e21_5078eef7).
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InitializeAndCleanup DISABLED_InitializeAndCleanup
#else
#define MAYBE_InitializeAndCleanup InitializeAndCleanup
#endif
TEST(GlTestEnvironmentTest, MAYBE_InitializeAndCleanup) {
  GlTestEnvironment gl_test_environment(gfx::Size(100, 100));
  EXPECT_NE(gl_test_environment.GetFrameBufferForTesting(), 0u);
  EXPECT_EQ(glGetError(), (GLenum)GL_NO_ERROR);
  // We just test that clean up doesn't crash.
}

}  // namespace vr
