// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_DEATH_TEST_MIXIN_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_DEATH_TEST_MIXIN_H_

namespace chrome_test_utils {

// Inherited by unit test fixtures that use death tests with a dependency on
// TestingBrowserProcess. Death tests incompletely initialize the test
// environment, so this mixin ensures g_browser_process is set up.
// TODO(crbug.com/487292986): Eliminate once tests avoid listener-based setup.
class TestingBrowserProcessDeathTestMixin {
 public:
  TestingBrowserProcessDeathTestMixin();
  ~TestingBrowserProcessDeathTestMixin() = default;
};

}  // namespace chrome_test_utils

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_DEATH_TEST_MIXIN_H_
