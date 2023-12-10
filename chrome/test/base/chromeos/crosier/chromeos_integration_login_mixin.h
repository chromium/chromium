// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_

#include <memory>
#include <string>

#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace {
class FakeSessionManagerClientBrowserHelper;
}

// Provides login supports to ChromeOS integration tests.
class ChromeOSIntegrationLoginMixin : public InProcessBrowserTestMixin {
 public:
  enum class Mode {
    // Mode starts from the chrome restart code path. It skips the login screen
    // and starts a stub user session automatically. It does not run session
    // manager daemon so it will not notify system daemons about user sign-in
    // state. It is intended for tests that do not care about login and only
    // test states in chrome.
    kStubLogin,

    // Mode starts from the login screen and uses test api to login. It starts
    // session_manager daemon and does cryptohome mount like production chrome.
    // Tests that do not depend on gaia identity should use this mode.
    kTestLogin,

    // Mode is the same as `kTestLogin` except that it uses the production gaia
    // server to authenticate. Tests that need gaia identity should use this
    // mode.
    kGaiaLogin,
  };

  explicit ChromeOSIntegrationLoginMixin(InProcessBrowserTestMixinHost* host);
  ChromeOSIntegrationLoginMixin(const ChromeOSIntegrationLoginMixin&) = delete;
  ChromeOSIntegrationLoginMixin& operator=(
      const ChromeOSIntegrationLoginMixin&) = delete;
  ~ChromeOSIntegrationLoginMixin() override;

  // Set the login mode. Must be called before SetUp.
  void SetMode(Mode mode);

  // Signs in a test account. For kTestLogin, it is a fake testuser@gmail.com.
  // For kGaiaLogin, the account is randomly picked from `gaiaPoolDefault`.
  void Login();

  // Returns true if cryptohome for `username_` is mounted.
  bool IsCryptohomeMounted() const;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  bool ShouldStartLoginScreen() const;
  void PrepareForNewUserLogin();

  // Performs login for kTestLogin mode by using Oobe test api.
  void DoTestLogin();

  // Performs login for kGaiaLogin mode by authenticating through production
  // gaia server.
  void DoGaiaLogin();

  bool setup_called_ = false;

  Mode mode_ = Mode::kStubLogin;

  // Username used to login. Only set for kTestLogin and kGaiaLogin.
  std::string username_;

  std::unique_ptr<FakeSessionManagerClientBrowserHelper>
      fake_session_manager_client_helper_;
  TestSudoHelperClient sudo_helper_client_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_
