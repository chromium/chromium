// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_
#define CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/test/vr_test_suite.h"

namespace gl {
class GLDisplay;
}  // namespace gl

namespace vr {

class VrGlTestSuite : public VrTestSuite {
 public:
  VrGlTestSuite(int argc, char** argv);
  void Initialize() override;
  void Shutdown() override;

 private:
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_
