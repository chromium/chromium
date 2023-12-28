// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace {
class FakeSessionManagerClientBrowserHelper;
}

// Interface that should be implemented by tests that need to login using
// accounts other than regular consumer accounts.
class CustomGaiaLoginDelegate {
 public:
  CustomGaiaLoginDelegate() = default;
  CustomGaiaLoginDelegate(const CustomGaiaLoginDelegate&) = delete;
  CustomGaiaLoginDelegate& operator=(const CustomGaiaLoginDelegate&) = delete;
  virtual ~CustomGaiaLoginDelegate() = default;

  // Sets username used to login and conducts login by navigating production
  // GAIA.
  virtual void DoCustomGaiaLogin(std::string& out_username) = 0;
};

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
    // server to authenticate. Tests using a regular consumer account that need
    // gaia identity should use this mode.
    kGaiaLogin,

    // Mode is similar to `kGaiaLogin` except that it uses customizations via a
    // CustomGaiaLoginDelegate for logging in with accounts other than regular
    // consumer users.
    kCustomGaiaLogin,
  };

  explicit ChromeOSIntegrationLoginMixin(InProcessBrowserTestMixinHost* host);
  ChromeOSIntegrationLoginMixin(const ChromeOSIntegrationLoginMixin&) = delete;
  ChromeOSIntegrationLoginMixin& operator=(
      const ChromeOSIntegrationLoginMixin&) = delete;
  ~ChromeOSIntegrationLoginMixin() override;

  // Set the login mode. Must be called before SetUp.
  void SetMode(Mode mode);

  // Returns if the login mode is using production GAIA.
  bool IsGaiaLoginMode() const;

  // Signs in a test account. For kTestLogin, it is a fake testuser@gmail.com.
  // For kGaiaLogin, the account is randomly picked from `gaiaPoolDefault`.
  void Login();

  // Returns true if cryptohome for `username_` is mounted.
  bool IsCryptohomeMounted() const;

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  Mode mode() const { return mode_; }

  void set_custom_gaia_login_delegate(CustomGaiaLoginDelegate* delegate) {
    gaia_login_delegate_ = delegate;
  }

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

  // Username used to login. Only set for kTestLogin, kGaiaLogin, and
  // kCustomGaiaLogin.
  std::string username_;

  // Used to conduct login for non regular consumer accounts. This should be
  // owned by tests using the mixin.
  raw_ptr<CustomGaiaLoginDelegate> gaia_login_delegate_ = nullptr;

  std::unique_ptr<FakeSessionManagerClientBrowserHelper>
      fake_session_manager_client_helper_;
  TestSudoHelperClient sudo_helper_client_;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_CHROMEOS_INTEGRATION_LOGIN_MIXIN_H_
