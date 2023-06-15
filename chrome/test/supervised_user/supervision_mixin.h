// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace supervised_user {

// This mixin is responsible for setting the user supervision status.
// The account is identified by a supplied email (account name).
class SupervisionMixin : public InProcessBrowserTestMixin {
 public:
  // Indicates what account type to use for the signed-in user.
  enum class AccountType {
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
    AccountType account_type = AccountType::kRegular;

    // Options for dependent mixins.
    EmbeddedTestServerSetupMixin::Options embedded_test_server_options = {};

    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165
    static Options Default() noexcept { return {}; }
  };

  SupervisionMixin() = delete;
  SupervisionMixin(InProcessBrowserTestMixinHost& test_mixin_host,
                   InProcessBrowserTest* test_base,
                   const Options& options = Options::Default());
  SupervisionMixin(InProcessBrowserTestMixinHost& test_mixin_host,
                   InProcessBrowserTest* test_base,
                   base::raw_ptr<net::EmbeddedTestServer> embedded_test_server,
                   const Options& options = Options::Default());

  SupervisionMixin(const SupervisionMixin&) = delete;
  SupervisionMixin& operator=(const SupervisionMixin&) = delete;
  ~SupervisionMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  signin::IdentityTestEnvironment* GetIdentityTestEnvironment() const;

 private:
  void SetUpTestServer();
  void SetUpIdentityTestEnvironment();
  void LogInUser();

  Profile* GetProfile() const;

  // This mixin dependencies.
  raw_ptr<InProcessBrowserTest> test_base_;
  FakeGaiaMixin fake_gaia_mixin_;
  absl::optional<EmbeddedTestServerSetupMixin>
      embedded_test_server_setup_mixin_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::CallbackListSubscription subscription_;

  // Test harness properties.
  signin::ConsentLevel consent_level_;
  std::string email_;
  AccountType account_type_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_SUPERVISION_MIXIN_H_
