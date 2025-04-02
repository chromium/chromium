// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_UNIT_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROME_UNIT_TEST_SUITE_H_

#include "base/test/test_discardable_memory_allocator.h"
#include "chrome/test/base/chrome_test_suite.h"

// Test suite for unit tests. Creates additional stub services that are not
// needed for browser tests (e.g. a TestingBrowserProcess).
class ChromeUnitTestSuite : public ChromeTestSuite {
 public:
  ChromeUnitTestSuite(int argc, char** argv);
  ChromeUnitTestSuite(const ChromeUnitTestSuite&) = delete;
  ChromeUnitTestSuite& operator=(const ChromeUnitTestSuite&) = delete;
  ~ChromeUnitTestSuite() override = default;

  // base::TestSuite overrides:
  void Initialize() override;
  void Shutdown() override;

  // These methods allow unit tests which run in the browser_test binary, and so
  // which don't exercise the initialization in this test suite, to do basic
  // setup which this class does.
  static void InitializeProviders();
  static void InitializeResourceBundle();

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

#endif  // CHROME_TEST_BASE_CHROME_UNIT_TEST_SUITE_H_
