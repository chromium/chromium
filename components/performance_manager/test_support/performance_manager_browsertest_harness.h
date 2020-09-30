// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_

#include "content/public/test/content_browser_test.h"

namespace performance_manager {

// Like PerformanceManagerTestHarness, but for browser tests. Full process
// trees and live RFHs, etc, are created. Meant to be used from
// components_browsertests and browser_tests.
class PerformanceManagerBrowserTestHarness
    : public content::ContentBrowserTest {
  using Super = content::ContentBrowserTest;

 public:
  PerformanceManagerBrowserTestHarness() = default;
  PerformanceManagerBrowserTestHarness(
      const PerformanceManagerBrowserTestHarness&) = delete;
  PerformanceManagerBrowserTestHarness& operator=(
      const PerformanceManagerBrowserTestHarness&) = delete;
  ~PerformanceManagerBrowserTestHarness() override;

  // content::BrowserTestBase:
  void PreRunTestOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Creates a content shell with its own window, hosting a single tab that is
  // navigated to about:blank. The WebContents will have the PM helpers
  // attached. Ownership of the shell rests with this object. Note that such a
  // shell is created by default by the fixture, accessible via "shell()". Only
  // call this if you need multiple independent WebContents.
  content::Shell* CreateShell();

  // Starts a navigation for the given |contents|.
  void StartNavigation(content::WebContents* contents, const GURL& url);

  // Waits for an ongoing navigation to terminate on the given |contents|.
  void WaitForLoad(content::WebContents* contents);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PERFORMANCE_MANAGER_BROWSERTEST_HARNESS_H_
