// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CHROMEOS_COMPONENTS_TEST_SUITE_H_
#define CHROMEOS_COMPONENTS_CHROMEOS_COMPONENTS_TEST_SUITE_H_

#include "base/test/test_suite.h"

class ChromeosComponentsTestSuite : public base::TestSuite {
 public:
  ChromeosComponentsTestSuite(int argc, char** argv);

  ChromeosComponentsTestSuite(const ChromeosComponentsTestSuite&) = delete;
  ChromeosComponentsTestSuite& operator=(const ChromeosComponentsTestSuite&) =
      delete;

  ~ChromeosComponentsTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

#endif  // CHROMEOS_COMPONENTS_CHROMEOS_COMPONENTS_TEST_SUITE_H_
