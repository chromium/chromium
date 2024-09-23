// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"

//
// InProcessBrowserTestMixin enables writing isolated test helpers which depend
// on the standard test lifecycle but should not be test bases.
//
// A new mixin is created by deriving from InProcessBrowserTestMixin and
// overriding methods as needed.
//
//   class MyMixin : public InProcessBrowserTestMixin {
//    public:
//     explicit MyMixin(InProcessBrowserTestMixinHost* host)
//         : InProcessBrowserTestMixin(host) {}
//      MyMixin(const MyMixin&) = delete;
//      MyMixin& operator=(const MyMixin&) = delete;
//     ~MyMixin() override = default;
//
//     // InProcessBrowserTestMixin:
//     void SetUpCommandLine(base::CommandLine* command_line) { /* ... */ }
//
//   };
//
//
// To use the mixin, declare it as a member variable on the class and call the
// constructor with the InProcessBrowserTestMixinHost also declared on the class
// (or parent class). The mixin will register itself with the host and the host
// will invoke all registered mixin methods.
//
// For example, here is how to use MixinBasedInProcessBrowserTest:
//
//   class SimpleUsage : public MixinBasedInProcessBrowserTest {
//    public:
//     SimpleUsage() = default;
//     SimpleUsage(const SimpleUsage&) = delete;
//     SimpleUsage& operator=(const SimpleUsage&) = delete;
//     ~SimpleUsage() override = default;
//
//    private:
//     MyMixin my_mixin_{&mixin_host_};
//     SomeOtherMixin some_other_mixin_{&mixin_host_};
//   };
//
//
// See WizardInProcessBrowserTest for an example of how to correctly embed a
// mixin host.
//

class InProcessBrowserTestMixinHost;

// Derive from this type to create a class which depends on the test lifecycle
// without also becoming a test base.
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
  //   CreatedBrowserMainParts
  //   SetUpOnMainThread
  //   PostRunTestOnMainThread
  //   TearDownOnMainThread
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
  virtual void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts);
  virtual void SetUpOnMainThread();
  virtual void PostRunTestOnMainThread();
  virtual void TearDownOnMainThread();
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
  void CreatedBrowserMainParts(content::BrowserMainParts* browser_main_parts);
  void SetUpOnMainThread();
  void PostRunTestOnMainThread();
  void TearDownOnMainThread();
  void TearDownInProcessBrowserTestFixture();
  void TearDown();

 private:
  // The constructor of InProcessBrowserTestMixin injects itself directly into
  // mixins_. This is done instead of an explicit AddMixin to make API usage
  // simpler.
  friend class InProcessBrowserTestMixin;

  std::vector<raw_ptr<InProcessBrowserTestMixin, VectorExperimental>> mixins_;
};

// An InProcessBrowserTest which supports mixins.
class MixinBasedInProcessBrowserTest : public InProcessBrowserTest {
 public:
  MixinBasedInProcessBrowserTest();
  MixinBasedInProcessBrowserTest(const MixinBasedInProcessBrowserTest&) =
      delete;
  MixinBasedInProcessBrowserTest& operator=(
      const MixinBasedInProcessBrowserTest&) = delete;
  ~MixinBasedInProcessBrowserTest() override;

  // InProcessBrowserTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;
  void PostRunTestOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;
  void TearDown() override;

 protected:
  InProcessBrowserTestMixinHost mixin_host_;
};

#endif  // CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_
