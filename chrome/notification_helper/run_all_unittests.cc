// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "chrome/install_static/test/scoped_install_details.h"

int main(int argc, char** argv) {
  install_static::ScopedInstallDetails scoped_install_details;

  base::TestSuite test_suite(argc, argv);

  // Since these tests will mutate machine state, they will likely interfere
  // with each other if they run in parallel. Therefore, run the tests serially.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
