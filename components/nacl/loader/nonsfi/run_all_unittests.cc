// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/rand_util.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int RunAllTestsImpl() {
  return RUN_ALL_TESTS();
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  testing::InitGoogleTest(&argc, argv);

  // Force early initialisation of /dev/urandom FD as it can't be initialised
  // from a sandbox.
  base::GetUrandomFD();

  return base::LaunchUnitTests(argc, argv, base::BindOnce(&RunAllTestsImpl));
}
