// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/components_test_suite.h"

int main(int argc, char** argv) {
  return base::LaunchUnitTestsSerially(argc, argv,
                                       GetLaunchCallback(argc, argv));
}
