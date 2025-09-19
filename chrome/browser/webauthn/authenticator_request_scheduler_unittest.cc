// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_scheduler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/common/features.h"

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
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(first_request);

  ASSERT_FALSE(AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetPrimaryMainFrame()));

  first_request.reset();
  ASSERT_TRUE(AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetPrimaryMainFrame()));
}

TEST_F(AuthenticatorRequestSchedulerTest,
       TwoWebContents_TwoSimultaneousRequests) {
  auto first_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetPrimaryMainFrame());

  auto second_web_contents = CreateTestWebContents();
  auto second_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      second_web_contents->GetPrimaryMainFrame());

  ASSERT_TRUE(first_request);
  ASSERT_TRUE(second_request);
}

class AuthenticatorRequestSchedulerFencedFramesTest
    : public AuthenticatorRequestSchedulerTest {
 public:
  AuthenticatorRequestSchedulerFencedFramesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~AuthenticatorRequestSchedulerFencedFramesTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AuthenticatorRequestSchedulerFencedFramesTest,
       SingleWebContents_SimultaneousRequestInFencedFrame) {
  // Navigate to an initial page.
  NavigateAndCommit(GURL("https://example.com"));

  auto first_request = AuthenticatorRequestScheduler::CreateRequestDelegate(
      web_contents()->GetPrimaryMainFrame());

  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  auto second_request =
      AuthenticatorRequestScheduler::CreateRequestDelegate(fenced_frame_root);

  ASSERT_TRUE(first_request);
  ASSERT_FALSE(second_request);
}

class AuthenticatorRequestSchedulerPrerenderTest
    : public AuthenticatorRequestSchedulerTest {
 public:
  AuthenticatorRequestSchedulerPrerenderTest() = default;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(AuthenticatorRequestSchedulerPrerenderTest,
       SingleWebContents_OneRequestInPrerendering) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  // Navigate to an initial page.
  NavigateAndCommit(GURL("https://example.com"));

  // Set prerendering loading.
  const GURL prerender_url("https://example.com/?prerendering");
  auto* prerender_rfh = content::WebContentsTester::For(web_contents())
                            ->AddPrerenderAndCommitNavigation(prerender_url);
  DCHECK_NE(prerender_rfh, nullptr);

  ASSERT_FALSE(
      AuthenticatorRequestScheduler::CreateRequestDelegate(prerender_rfh));
}
