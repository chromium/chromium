// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_COMPONENTS_TEST_SUITE_H_
#define COMPONENTS_TEST_COMPONENTS_TEST_SUITE_H_

#include "base/test/launcher/unit_test_launcher.h"

base::RunTestSuiteCallback GetLaunchCallback(int argc, char** argv);

#endif  // COMPONENTS_TEST_COMPONENTS_TEST_SUITE_H_
