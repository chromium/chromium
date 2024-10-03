// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_
#define CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_

#include <optional>

#include "base/functional/function_ref.h"
#include "base/process/process.h"
#include "build/build_config.h"

namespace enterprise_companion {

// Waits for a multi-process test child to exit without blocking the main
// sequence, returning its exit code. Expects the process to exit within the
// test action timeout.
int WaitForProcess(base::Process&);

// Waits for a given `predicate` to become true. Invokes `still_waiting`
// periodically to provide a indication of progress. Returns true if the
// predicate becomes true before a timeout, otherwise returns false.
[[nodiscard]] bool WaitFor(
    base::FunctionRef<bool()> predicate,
    base::FunctionRef<void()> still_waiting = [] {});

#if BUILDFLAG(IS_WIN)
// Asserts that the application has been properly registered with the updater.
void ExpectUpdaterRegistration();

// Sets the given proxy settings via Group Policy.
void SetLocalProxyPolicies(
    std::optional<std::string> proxy_mode,
    std::optional<std::string> pac_url,
    std::optional<std::string> proxy_server,
    std::optional<bool> cloud_policy_overrides_platform_policy);
#endif

#if BUILDFLAG(IS_MAC)
// Install a fake ksadmin which produces an exit code determined by
// `should_succeed`.
void InstallFakeKSAdmin(bool should_succeed);
#endif

// Test methods which can be overridden for per-platform behavior.
class TestMethods {
 public:
  TestMethods() = default;
  virtual ~TestMethods() = default;

  // Removes traces of the application from the system.
  virtual void Clean();

  // Asserts the absence of traces of the application from the system.
  virtual void ExpectClean();

  // Asserts that the application has been installed.
  virtual void ExpectInstalled();

  // Installs the application under test via the bundled installer.
  virtual void Install();

  // Runs the "install if needed" command on the application under test.
  virtual void InstallIfNeeded();
};

TestMethods& GetTestMethods();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_TEST_TEST_UTILS_H_
