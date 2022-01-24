// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_scheduler.h"

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"

class AuthenticatorRequestSchedulerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AuthenticatorRequestSchedulerTest() = default;

  AuthenticatorRequestSchedulerTest(const AuthenticatorRequestSchedulerTest&) =
      delete;
  AuthenticatorRequestSchedulerTest& operator=(
      const AuthenticatorRequestSchedulerTest&) = delete;

  ~AuthenticatorRequestSchedulerTest() override = default;
};

TEST_F(AuthenticatorRequestSchedulerTest,
       SingleWebContents_AtMostOneSimultaneousRequest) {
  auto first_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetMainFrame());
  ASSERT_TRUE(first_request);

  ASSERT_FALSE(AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetMainFrame()));

  first_request.reset();
  ASSERT_TRUE(AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetMainFrame()));
}

TEST_F(AuthenticatorRequestSchedulerTest,
       TwoWebContents_TwoSimultaneousRequests) {
  auto first_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetMainFrame());

  auto second_web_contents = CreateTestWebContents();
  auto second_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      second_web_contents->GetMainFrame());

  ASSERT_TRUE(first_request);
  ASSERT_TRUE(second_request);
}
