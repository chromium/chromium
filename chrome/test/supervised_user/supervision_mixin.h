// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/api_mock_setup_mixin.h"
#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"
#include "chrome/test/supervised_user/google_auth_state_waiter_mixin.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace supervised_user {

// This mixin is responsible for setting the user supervision status.
// The account is identified by a supplied email (account name).
class SupervisionMixin : public InProcessBrowserTestMixin {
 public:
  // Indicates whether to sign-in, and with what type of account.
  enum class SignInMode {
    kSignedOut,
    kRegular,
    kSupervised,
  };

  // Use options class pattern to avoid growing list of arguments
  // go/cpp-primer#options_pattern and take advantage of auto-generated default
  // constructor.
  struct Options {
    // Account creation properties.
    signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin;
    std::string email = "test@gmail.com";
    SignInMode sign_in_mode = SignInMode::kRegular;

    // Options for dependent mixins.
    EmbeddedTestServerSetupMixin::Options embedded_test_server_options = {};

    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165
    static Options Default() noexcept { return {}; }
  };

  SupervisionMixin() = delete;

  // The `embedded_test_server` is used to configure FakeGaia handlers and to
  // add additional resolver rules.
  SupervisionMixin(InProcessBrowserTestMixinHost& test_mixin_host,
                   InProcessBrowserTest* test_base,
                   raw_ptr<net::EmbeddedTestServer> embedded_test_server,
                   const Options& options = Options::Default());

  SupervisionMixin(const SupervisionMixin&) = delete;
  SupervisionMixin& operator=(const SupervisionMixin&) = delete;
  ~SupervisionMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  signin::IdentityTestEnvironment* GetIdentityTestEnvironment() const;

  // Controls FakeGaia's response.
  void SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus status);

  // Sign in (assumes that the current state is signed out). The `mode` argument
  // must not be `kSignedOut`.
  void SignIn(SignInMode mode);

  // Invalidates the access token and Google cookie to put the primary user
  // in the pending state.
  void SetPendingStateForPrimaryAccount();

  EmbeddedTestServerSetupMixin& embedded_test_server_setup_mixin() {
    return *embedded_test_server_setup_mixin_;
  }

  KidsManagementApiMockSetupMixin& api_mock_setup_mixin() {
    return api_mock_setup_mixin_;
  }

  // TODO(b/364011203): make this private once auth state waiting is handled by
  // GoogleAuthStateWaiterMixin on Windows too.
  static ChildAccountService::AuthState GetExpectedAuthState(
      SignInMode sign_in_mode);

 private:
  void SetUpTestServer();
  void SetUpIdentityTestEnvironment();
  void ConfigureIdentityTestEnvironment();
  void ConfigureParentalControls(bool is_supervised_profile);
  void SetParentalControlsAccountCapability(bool is_supervised_profile);

  Profile* GetProfile() const;

  // This mixin dependencies.
  raw_ptr<InProcessBrowserTest> test_base_;
  FakeGaiaMixin fake_gaia_mixin_;
  std::optional<EmbeddedTestServerSetupMixin> embedded_test_server_setup_mixin_;
  KidsManagementApiMockSetupMixin api_mock_setup_mixin_;
  GoogleAuthStateWaiterMixin google_auth_state_waiter_mixin_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::CallbackListSubscription subscription_;

  // Test harness properties.
  signin::ConsentLevel consent_level_;
  std::string email_;
  SignInMode sign_in_mode_;
};

// Enables user-readable output from gtest (instead of binary streams).
std::ostream& operator<<(std::ostream& stream,
                         SupervisionMixin::SignInMode sign_in_mode);
std::string SignInModeAsString(SupervisionMixin::SignInMode sign_in_mode);

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_
