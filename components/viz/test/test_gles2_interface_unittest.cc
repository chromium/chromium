// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gles2_interface.h"

#include <memory>

#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace viz {
namespace {

TEST(TestGLES2InterfaceTest, UseMultipleRenderAndFramebuffers) {
  auto gl = std::make_unique<TestGLES2Interface>();

  GLuint ids[2];
  gl->GenFramebuffers(2, ids);
  EXPECT_NE(ids[0], ids[1]);
  gl->DeleteFramebuffers(2, ids);

  gl->GenRenderbuffers(2, ids);
  EXPECT_NE(ids[0], ids[1]);
  gl->DeleteRenderbuffers(2, ids);
}

}  // namespace
}  // namespace viz
