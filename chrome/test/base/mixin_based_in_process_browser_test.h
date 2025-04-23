// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_

#include <concepts>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

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
class PrefService;

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

template <typename Fixture>
#if BUILDFLAG(IS_ANDROID)
  requires std::derived_from<Fixture, AndroidBrowserTest>
#else
  requires std::derived_from<Fixture, InProcessBrowserTest>
#endif
class InProcessBrowserTestMixinHostSupport : public Fixture {
 public:
  // Fixture:
  void SetUp() override {
    mixin_host_.SetUp();
    Fixture::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpCommandLine(command_line);
    Fixture::SetUpCommandLine(command_line);
  }
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpDefaultCommandLine(command_line);
    Fixture::SetUpDefaultCommandLine(command_line);
  }
  bool SetUpUserDataDirectory() override {
    return mixin_host_.SetUpUserDataDirectory() &&
           Fixture::SetUpUserDataDirectory();
  }
  void SetUpInProcessBrowserTestFixture() override {
    mixin_host_.SetUpInProcessBrowserTestFixture();
    Fixture::SetUpInProcessBrowserTestFixture();
  }
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    mixin_host_.SetUpLocalStatePrefService(local_state);
    Fixture::SetUpLocalStatePrefService(local_state);
  }
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    mixin_host_.CreatedBrowserMainParts(browser_main_parts);
    Fixture::CreatedBrowserMainParts(browser_main_parts);
  }
  void SetUpOnMainThread() override {
    mixin_host_.SetUpOnMainThread();
    Fixture::SetUpOnMainThread();
  }
  void TearDownOnMainThread() override {
    mixin_host_.TearDownOnMainThread();
    Fixture::TearDownOnMainThread();
  }
  void PostRunTestOnMainThread() override {
    mixin_host_.PostRunTestOnMainThread();
    Fixture::PostRunTestOnMainThread();
  }
  void TearDownInProcessBrowserTestFixture() override {
    mixin_host_.TearDownInProcessBrowserTestFixture();
    Fixture::TearDownInProcessBrowserTestFixture();
  }
  void TearDown() override {
    mixin_host_.TearDown();
    Fixture::TearDown();
  }

 protected:
  InProcessBrowserTestMixinHost mixin_host_;
};

#if BUILDFLAG(IS_ANDROID)
// An AndroidBrowserTest which supports mixins.
using MixinBasedAndroidBrowserTest =
    InProcessBrowserTestMixinHostSupport<AndroidBrowserTest>;
// The implementation is included in mixin_based_in_process_browser_test.cc
extern template class InProcessBrowserTestMixinHostSupport<AndroidBrowserTest>;
#else
// An InProcessBrowserTest which supports mixins.
using MixinBasedInProcessBrowserTest =
    InProcessBrowserTestMixinHostSupport<InProcessBrowserTest>;
// The implementation is included in mixin_based_in_process_browser_test.cc
extern template class InProcessBrowserTestMixinHostSupport<
    InProcessBrowserTest>;
#endif

#endif  // CHROME_TEST_BASE_MIXIN_BASED_IN_PROCESS_BROWSER_TEST_H_
