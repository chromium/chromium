// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_suite_aura.h"

#include "ui/gl/test/gl_surface_test_support.h"

namespace exo {
namespace test {

ExoTestSuiteAura::ExoTestSuiteAura(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

ExoTestSuiteAura::~ExoTestSuiteAura() = default;

void ExoTestSuiteAura::Initialize() {
  base::TestSuite::Initialize();
  display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
}

void ExoTestSuiteAura::Shutdown() {
  gl::GLSurfaceTestSupport::ShutdownGL(display_);
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace exo
