// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/gl_test_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace vr {

// TODO(crbug.com/1394319): Re-enable this test
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_InitializeAndCleanup DISABLED_InitializeAndCleanup
#else
#define MAYBE_InitializeAndCleanup InitializeAndCleanup
#endif
TEST(GlTestEnvironmentTest, MAYBE_InitializeAndCleanup) {
#if BUILDFLAG(IS_WIN)
  // VR is not supported on Windows 7.
  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return;
#endif
  GlTestEnvironment gl_test_environment(gfx::Size(100, 100));
  EXPECT_NE(gl_test_environment.GetFrameBufferForTesting(), 0u);
  EXPECT_EQ(glGetError(), (GLenum)GL_NO_ERROR);
  // We just test that clean up doesn't crash.
}

}  // namespace vr
