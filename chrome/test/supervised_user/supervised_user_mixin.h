// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_SUPERVISED_USER_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_SUPERVISED_USER_MIXIN_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_test_utils.h"

namespace supervised_user {

// This mixin is responsible for setting the user supervision status.
// The account is identified by a supplied email (account name).
class SupervisedUserMixin : public InProcessBrowserTestMixin {
 public:
  // Indicates what account type to use for the signed-in user.
  enum AccountType {
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
    AccountType account_type = kRegular;
  };

  SupervisedUserMixin() = delete;
  SupervisedUserMixin(InProcessBrowserTestMixinHost& test_mixin_host,
                      InProcessBrowserTest* test_base);
  SupervisedUserMixin(InProcessBrowserTestMixinHost& test_mixin_host,
                      InProcessBrowserTest* test_base,
                      const Options& options);

  SupervisedUserMixin(const SupervisedUserMixin&) = delete;
  SupervisedUserMixin& operator=(const SupervisedUserMixin&) = delete;
  ~SupervisedUserMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  FakeGaiaMixin& GetFakeGaiaMixin() { return fake_gaia_mixin_; }

  signin::IdentityTestEnvironment* identity_test_environment();

 private:
  // This mixin dependencies.
  raw_ptr<InProcessBrowserTest> test_base_;
  FakeGaiaMixin fake_gaia_mixin_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::CallbackListSubscription subscription_;
  base::test::ScopedFeatureList feature_list_{
      kEnableSupervisionOnDesktopAndIOS};

  // Test harness properties.
  signin::ConsentLevel consent_level_;
  std::string email_;
  AccountType account_type_;

  TestingProfile* testing_profile();

  void SetUpTestServer();
  void SetUpIdentityTestEnvironment();
  void LogInUser();
  void SetSupervision(bool is_supervised);
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_SUPERVISED_USER_MIXIN_H_
