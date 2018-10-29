// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_
#define CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_

#include "chrome/browser/vr/test/vr_test_suite.h"

namespace vr {

class VrGlTestSuite : public VrTestSuite {
 public:
  VrGlTestSuite(int argc, char** argv);
  void Initialize() override;
  void Shutdown() override;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_VR_GL_TEST_SUITE_H_
