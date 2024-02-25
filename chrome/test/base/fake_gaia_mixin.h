// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_FAKE_GAIA_MIXIN_H_
#define CHROME_TEST_BASE_FAKE_GAIA_MIXIN_H_

#include <initializer_list>
#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "google_apis/gaia/fake_gaia.h"

namespace base {
class CommandLine;
}

// Test mixin that simplifies the usage of `FakeGaia`.
//
// Simply create it in your test in order to configure Chrome to talk to the
// fake Gaia instead of attempting network requests to the real one. E.g.:
//
//   class MyTest : public MixinBasedInProcessBrowserTest {
//    private:
//     FakeGaiaMixin fake_gaia_{&mixin_host_};
//   };
class FakeGaiaMixin : public InProcessBrowserTestMixin {
 public:
  using UiPath = std::initializer_list<base::StringPiece>;

  // Default fake user email and password, may be used by tests.
  static const char kFakeUserEmail[];
  static const char kFakeUserPassword[];
  static const char kFakeUserGaiaId[];
  static const char kFakeAuthCode[];
  static const char kFakeRefreshToken[];
  static const char kEmptyUserServices[];
  static const char kFakeAllScopeAccessToken[];

  // How many seconds until the fake access tokens expire.
  static const int kFakeAccessTokenExpiration;

  // FakeGaia is configured to return these cookies for kFakeUserEmail.
  static const char kFakeSIDCookie[];
  static const char kFakeLSIDCookie[];

  // For obviously consumer users (that have e.g. @gmail.com e-mail) policy
  // fetching code is skipped. This code is executed only for users that may be
  // enterprise users. Thus if you derive from this class and don't need
  // policies, please use @gmail.com e-mail for login. But if you need policies
  // for your test, you must use e-mail addresses that a) have a potentially
  // enterprise domain and b) have been registered with `fake_gaia_`.
  // For your convenience, the e-mail addresses for users that have been set up
  // in this way are provided below.
  static const char kEnterpriseUser1[];
  static const char kEnterpriseUser1GaiaId[];
  static const char kEnterpriseUser2[];
  static const char kEnterpriseUser2GaiaId[];

  static const char kTestUserinfoToken1[];
  static const char kTestRefreshToken1[];
  static const char kTestUserinfoToken2[];
  static const char kTestRefreshToken2[];

  static const UiPath kEmailPath;
  static const UiPath kPasswordPath;

  explicit FakeGaiaMixin(InProcessBrowserTestMixinHost* host);

  FakeGaiaMixin(const FakeGaiaMixin&) = delete;
  FakeGaiaMixin& operator=(const FakeGaiaMixin&) = delete;

  ~FakeGaiaMixin() override;

  // Sets up fake gaia for the login code:
  // - Maps `user_email` to `gaia_id`. If `gaia_id` is empty, `user_email` will
  //   be mapped to kDefaultGaiaId in FakeGaia;
  // - Issues a special all-scope access token associated with the test refresh
  //   token;
  void SetupFakeGaiaForLogin(const std::string& user_email,
                             const std::string& gaia_id,
                             const std::string& refresh_token);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Sets up fake gaia to serve access tokens for a child user.
  // *   Maps `user_email` to `gaia_id`. If `gaia_id` is empty, `user_email`
  //     will be mapped to kDefaultGaiaId in FakeGaia.
  // *   Issues user info token scoped for device management service.
  // *   If `issue_any_scope_token`, issues a special all-access token
  //     associated with the test refresh token (as it's done in
  //     SetupFakeGaiaForLogin()).
  // *   Initializes fake merge session as needed.
  void SetupFakeGaiaForChildUser(const std::string& user_email,
                                 const std::string& gaia_id,
                                 const std::string& refresh_token,
                                 bool issue_any_scope_token);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetupFakeGaiaForLoginManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bool initialize_configuration() { return initialize_configuration_; }
  void set_initialize_configuration(bool value) {
    initialize_configuration_ = value;
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  bool initialize_child_id_token() { return initialize_child_id_token_; }

  void set_initialize_child_id_token(bool value) {
    initialize_child_id_token_ = value;
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  FakeGaia* fake_gaia() { return fake_gaia_.get(); }
  net::EmbeddedTestServer* gaia_server() { return &gaia_server_; }

  // Returns the URL on `gaia_server()` for `relative_url`. Use this method
  // instead of `gaia_server()->GetURL()` so the hostname is correct.
  GURL GetFakeGaiaURL(const std::string& relative_url);

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  net::EmbeddedTestServer gaia_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<FakeGaia> fake_gaia_;
  bool initialize_configuration_ = true;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  bool initialize_child_id_token_ = false;
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
};

#endif  // CHROME_TEST_BASE_FAKE_GAIA_MIXIN_H_
