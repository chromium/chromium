// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_MIXIN_H_
#define CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_MIXIN_H_

#include <vector>

#include "base/memory/raw_ptr.h"

namespace base {
class CommandLine;
}

namespace content {
class BrowserMainParts;
}

class InProcessBrowserTestMixinHost;
class PrefService;

// Derive from this type to create a class which depends on the test lifecycle
// without also becoming a test base.
//
// See mixin_based_in_process_browser_test.h for full documentation and
// examples of how to use mixins with MixinBasedInProcessBrowserTest.
class InProcessBrowserTestMixin {
 public:
  explicit InProcessBrowserTestMixin(InProcessBrowserTestMixinHost* host);
  InProcessBrowserTestMixin(const InProcessBrowserTestMixin&) = delete;
  InProcessBrowserTestMixin& operator=(const InProcessBrowserTestMixin&) =
      delete;
  virtual ~InProcessBrowserTestMixin() = default;

  // See InProcessBrowserTest for docs. The call order is:
  //
  // SetUp
  //   SetUpCommandLine
  //   SetUpDefaultCommandLine
  //   SetUpUserDataDirectory
  //   SetUpInProcessBrowserTestFixture
  //   SetUpLocalStatePrefService
  //   CreatedBrowserMainParts
  //   SetUpOnMainThread
  //   TearDownOnMainThread
  //   PostRunTestOnMainThread
  //   TearDownInProcessBrowserTestFixture
  //   TearDown
  //
  // SetUp is the function which calls SetUpCommandLine,
  // SetUpDefaultCommandLine, etc.
  virtual void SetUp();
  virtual void SetUpCommandLine(base::CommandLine* command_line);
  virtual void SetUpDefaultCommandLine(base::CommandLine* command_line);
  virtual bool SetUpUserDataDirectory();
  virtual void SetUpInProcessBrowserTestFixture();
  virtual void SetUpLocalStatePrefService(PrefService* local_state);
  virtual void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts);
  virtual void SetUpOnMainThread();
  virtual void TearDownOnMainThread();
  virtual void PostRunTestOnMainThread();
  virtual void TearDownInProcessBrowserTestFixture();
  virtual void TearDown();
};

// The mixin host executes the callbacks on the mixin instances.
class InProcessBrowserTestMixinHost final {
 public:
  InProcessBrowserTestMixinHost();
  InProcessBrowserTestMixinHost(const InProcessBrowserTestMixinHost&) = delete;
  InProcessBrowserTestMixinHost& operator=(
      const InProcessBrowserTestMixinHost&) = delete;
  ~InProcessBrowserTestMixinHost();

  void SetUp();
  void SetUpCommandLine(base::CommandLine* command_line);
  void SetUpDefaultCommandLine(base::CommandLine* command_line);
  bool SetUpUserDataDirectory();
  void SetUpInProcessBrowserTestFixture();
  void SetUpLocalStatePrefService(PrefService* local_state);
  void CreatedBrowserMainParts(content::BrowserMainParts* browser_main_parts);
  void SetUpOnMainThread();
  void TearDownOnMainThread();
  void PostRunTestOnMainThread();
  void TearDownInProcessBrowserTestFixture();
  void TearDown();

 private:
  // The constructor of InProcessBrowserTestMixin injects itself directly into
  // mixins_. This is done instead of an explicit AddMixin to make API usage
  // simpler.
  friend class InProcessBrowserTestMixin;

  std::vector<raw_ptr<InProcessBrowserTestMixin, VectorExperimental>> mixins_;
};

#endif  // CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_MIXIN_H_
