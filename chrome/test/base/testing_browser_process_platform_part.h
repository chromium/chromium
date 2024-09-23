// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_

#include "chrome/browser/browser_process_platform_part.h"

// A TestingBrowserProcessPlatformPart is essentially a
// BrowserProcessPlatformPart except it doesn't have an OomPriorityManager on
// Chrome OS.
class TestingBrowserProcessPlatformPart : public BrowserProcessPlatformPart {
 public:
  TestingBrowserProcessPlatformPart();
  TestingBrowserProcessPlatformPart(const TestingBrowserProcessPlatformPart&) =
      delete;
  TestingBrowserProcessPlatformPart& operator=(
      const TestingBrowserProcessPlatformPart&) = delete;
  ~TestingBrowserProcessPlatformPart() override;
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_PLATFORM_PART_H_
