// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chrome/browser/vr/test/vr_gl_test_suite.h"

int main(int argc, char** argv) {
  vr::VrGlTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindRepeating(&vr::VrGlTestSuite::Run,
                          base::Unretained(&test_suite)));
}
