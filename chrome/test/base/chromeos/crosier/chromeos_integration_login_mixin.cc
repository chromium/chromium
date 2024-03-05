// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

#include "ash/constants/ash_switches.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/dbus/ash_dbus_helper.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/chromeos/crosier/gaia_host_util.h"
#include "chrome/test/base/chromeos/crosier/test_accounts.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

class FakeSessionManagerClientBrowserHelper
    : public ash::DBusHelperObserverForTest {
 public:
  FakeSessionManagerClientBrowserHelper() {
    ash::DBusHelperObserverForTest::Set(this);
  }
  FakeSessionManagerClientBrowserHelper(
      const FakeSessionManagerClientBrowserHelper&) = delete;
  FakeSessionManagerClientBrowserHelper& operator=(
      const FakeSessionManagerClientBrowserHelper&) = delete;
  ~FakeSessionManagerClientBrowserHelper() override {
    ash::DBusHelperObserverForTest::Set(nullptr);
  }

  // ash::DBusHelperObserverForTest:
  void PostInitializeDBus() override {
    // Create FakeSessionManageClient after real SessionManagerClient is created
    // and before it is referenced.
    scoped_fake_session_manager_client_.emplace();
    ash::FakeSessionManagerClient::Get()->set_stop_session_callback(
        base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
  }

  void PreShutdownDBus() override {
    // Release FakeSessionManagerClient shutting down dbus clients.
    scoped_fake_session_manager_client_.reset();
  }

 private:
  // Optionally, use FakeSessionManagerClient if a test only needs the stub
  // user session.
  std::optional<ash::ScopedFakeSessionManagerClient>
      scoped_fake_session_manager_client_;
};
}  // namespace

ChromeOSIntegrationLoginMixin::ChromeOSIntegrationLoginMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ChromeOSIntegrationLoginMixin::~ChromeOSIntegrationLoginMixin() {
  sudo_helper_client_.EnsureSessionManagerStopped();
}

void ChromeOSIntegrationLoginMixin::SetMode(Mode mode) {
  CHECK(!setup_called_);
  mode_ = mode;
}

bool ChromeOSIntegrationLoginMixin::IsGaiaLoginMode() const {
  return mode_ == Mode::kGaiaLogin || mode_ == Mode::kCustomGaiaLogin;
}

void ChromeOSIntegrationLoginMixin::Login() {
  switch (mode_) {
    case Mode::kStubLogin: {
      // Nothing to do since stub user should be signed in on start.
      break;
    }
    case Mode::kTestLogin: {
      DoTestLogin();
      break;
    }
    case Mode::kGaiaLogin: {
      DoGaiaLogin();
      break;
    }
    case Mode::kCustomGaiaLogin: {
      if (gaia_login_delegate_) {
        gaia_login_delegate_->DoCustomGaiaLogin(username_);
      } else {
        CHECK(false)
            << "CustomGaiaDelegate must be set for kCustomGaiaLogin mode.";
      }
      break;
    }
  }
}

bool ChromeOSIntegrationLoginMixin::IsCryptohomeMounted() const {
  // No cryptohome mount for kStubLogin.
  if (mode_ == Mode::kStubLogin) {
    return false;
  }

  if (username_.empty()) {
    return false;
  }

  user_data_auth::IsMountedRequest request;
  request.set_username(gaia::CanonicalizeEmail(gaia::SanitizeEmail(username_)));

  bool is_mounted = false;
  base::RunLoop run_loop;
  ash::UserDataAuthClient::Get()->IsMounted(
      request, base::BindLambdaForTesting(
                   [&](std::optional<user_data_auth::IsMountedReply> result) {
                     if (!result.has_value()) {
                       LOG(ERROR) << "Failed to call IsMounted.";
                       is_mounted = false;
                     } else {
                       is_mounted = result->is_mounted();
                     }

                     run_loop.Quit();
                   }));
  run_loop.Run();

  return is_mounted;
}

void ChromeOSIntegrationLoginMixin::SetUp() {
  setup_called_ = true;

  switch (mode_) {
    case Mode::kStubLogin: {
      fake_session_manager_client_helper_ =
          std::make_unique<FakeSessionManagerClientBrowserHelper>();
      break;
    }
    case Mode::kTestLogin: {
      [[fallthrough]];
    }
    case Mode::kGaiaLogin:
    case Mode::kCustomGaiaLogin: {
      PrepareForNewUserLogin();
      break;
    }
  }
}

void ChromeOSIntegrationLoginMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(
      ash::switches::kDisableHIDDetectionOnOOBEForTesting);

  if (!IsGaiaLoginMode()) {
    command_line->AppendSwitch(ash::switches::kDisableGaiaServices);
  }

  if (ShouldStartLoginScreen()) {
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(
        ash::switches::kDisableOOBEChromeVoxHintTimerForTesting);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
  }

  if (mode_ == Mode::kTestLogin) {
    // Needed to use Oobe test api to login.
    command_line->AppendSwitch(ash::switches::kEnableOobeTestAPI);
  }
}

bool ChromeOSIntegrationLoginMixin::ShouldStartLoginScreen() const {
  return mode_ == Mode::kTestLogin || IsGaiaLoginMode();
}

void ChromeOSIntegrationLoginMixin::PrepareForNewUserLogin() {
  CHECK(sudo_helper_client_.RunCommand("./reset_dut.py").return_code == 0);

  // Starts session_manager daemon and use `chrome::ExitIgnoreUnloadHandlers` as
  // the session_manager stopped callback. The callback would be invoked
  // when session_manager daemon terminates (getting a StopSession dbus call,
  // or crashed).
  //
  // In normal case, SessionManagerClient::StopSession would be called when a
  // test case finishes. The daemon would terminate and `test_sudo_helper.py`
  // script sends a message back to `TestSudoHelperClient`, which triggers the
  // callback.
  //
  // For the crashed case, the callback would be invoked before the test case
  // finishes and cause it to fail.
  auto result = sudo_helper_client_.StartSessionManager(
      base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
  CHECK_EQ(result.return_code, 0);
}

void ChromeOSIntegrationLoginMixin::DoTestLogin() {
  ash::test::WaitForOobeJSReady();

  // Any gmail account and password works for test login.
  constexpr char kTestUser[] = "testuser@gmail.com";
  constexpr char kTestPassword[] = "testpass";
  constexpr char kTestGaiaId[] = "12345";

  username_ = kTestUser;

  ash::test::OobeJS().Evaluate(
      base::StringPrintf("Oobe.loginForTesting(\"%s\", \"%s\",\"%s\")",
                         kTestUser, kTestPassword, kTestGaiaId));

  // Skip post login steps, such as ToS etc.
  ash::WizardController::default_controller()->SkipPostLoginScreensForTesting();
}

void ChromeOSIntegrationLoginMixin::DoGaiaLogin() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Skip to login screen.
  crosier::SkipToGaiaScreenAndWait();

  std::string email;
  std::string password;

  {
    // Allows reading account pool json file.
    base::ScopedAllowBlockingForTesting allow_blocking;

    crosier::GetGaiaTestAccount(email, password);
    CHECK(!email.empty() && !password.empty());
  }

  username_ = email;

  crosier::GaiaFrameJS()
      .CreateWaiter("!!document.querySelector('#identifierId')")
      ->Wait();
  crosier::GaiaFrameJS().Evaluate(base::StrCat(
      {"document.querySelector('#identifierId').value=\"", email, "\""}));
  ash::test::OobeJS().Evaluate("Oobe.clickGaiaPrimaryButtonForTesting()");

  crosier::GaiaFrameJS()
      .CreateWaiter("!!document.querySelector('input[type=password]')")
      ->Wait();
  crosier::GaiaFrameJS().Evaluate(
      base::StrCat({"document.querySelector('input[type=password]').value=\"",
                    password, "\""}));
  ash::test::OobeJS().Evaluate("Oobe.clickGaiaPrimaryButtonForTesting()");

  // Skip post login steps, such as ToS etc.
  ash::WizardController::default_controller()->SkipPostLoginScreensForTesting();
#else
  CHECK(false) << "Gaia login is only supported in branded build.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
