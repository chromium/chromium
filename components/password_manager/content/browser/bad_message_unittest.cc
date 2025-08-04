// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/bad_message.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager::bad_message {

class BadMessageTest : public content::RenderViewHostTestHarness {
 public:
  BadMessageTest() = default;
  ~BadMessageTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));
  }

 protected:
  content::RenderFrameHost* GetMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void ExpectNoHistogramEntries(const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount(
        "Stability.BadMessageTerminated.PasswordManager", 0);
  }

  void ExpectHistogramEntry(const base::HistogramTester& histogram_tester,
                            BadMessageReason reason) {
    histogram_tester.ExpectUniqueSample(
        "Stability.BadMessageTerminated.PasswordManager",
        static_cast<int>(reason), 1);
  }
};

// Tests that CheckForIllegalURL() allows legitimate HTTPS URLs to proceed.
TEST_F(BadMessageTest, CheckForIllegalURL_ValidURL) {
  base::HistogramTester histogram_tester;
  GURL valid_url("https://example.com/login");

  bool result =
      CheckForIllegalURL(GetMainFrame(), valid_url,
                         BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

// Tests that CheckForIllegalURL() blocks about: URLs and terminates the
// renderer. About URLs (like about:blank) are dangerous because they bypass
// normal web security constraints and could be exploited for privilege
// escalation attacks.
TEST_F(BadMessageTest, CheckForIllegalURL_AboutURL) {
  base::HistogramTester histogram_tester;
  GURL about_url("about:blank");

  bool result =
      CheckForIllegalURL(GetMainFrame(), about_url,
                         BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);

  EXPECT_FALSE(result);
  ExpectHistogramEntry(histogram_tester,
                       BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);
}

// Tests that CheckForIllegalURL() blocks data: URLs and terminates the
// renderer. Data URLs contain embedded content that bypasses origin
// restrictions and could be used to inject malicious content for password theft
// attacks.
TEST_F(BadMessageTest, CheckForIllegalURL_DataURL) {
  base::HistogramTester histogram_tester;
  GURL data_url("data:text/html,<html></html>");

  bool result = CheckForIllegalURL(
      GetMainFrame(), data_url,
      BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED);

  EXPECT_FALSE(result);
  ExpectHistogramEntry(
      histogram_tester,
      BadMessageReason::CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED);
}

// Tests that CheckChildProcessSecurityPolicyForURL() allows URLs when the
// renderer process has proper permissions.
TEST_F(BadMessageTest, CheckChildProcessSecurityPolicyForURL_ValidURL) {
  base::HistogramTester histogram_tester;
  GURL valid_url("https://example.com/login");

  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
      GetMainFrame()->GetProcess()->GetDeprecatedID(),
      url::Origin::Create(valid_url));

  bool result = CheckChildProcessSecurityPolicyForURL(
      GetMainFrame(), valid_url,
      BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

// Tests that CheckChildProcessSecurityPolicyForURL() delegates illegal URL
// detection to CheckForIllegalURL().
TEST_F(BadMessageTest, CheckChildProcessSecurityPolicyForURL_AboutURL) {
  base::HistogramTester histogram_tester;
  GURL about_url("about:blank");

  bool result = CheckChildProcessSecurityPolicyForURL(
      GetMainFrame(), about_url,
      BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);

  EXPECT_FALSE(result);
  ExpectHistogramEntry(histogram_tester,
                       BadMessageReason::CPMD_BAD_ORIGIN_FORM_SUBMITTED);
}

// Tests that CheckFrameNotPrerendering() allows active frames to proceed with
// password manager operations.
TEST_F(BadMessageTest, CheckFrameNotPrerendering_ActiveFrame) {
  base::HistogramTester histogram_tester;

  bool result = CheckFrameNotPrerendering(GetMainFrame());

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

// Tests that CheckGeneratedPassword() allows non-empty passwords to proceed.
TEST_F(BadMessageTest, CheckGeneratedPassword_ValidPassword) {
  base::HistogramTester histogram_tester;
  std::u16string valid_password = u"test_password123";

  bool result = CheckGeneratedPassword(GetMainFrame(), valid_password);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

// Tests that CheckGeneratedPassword() blocks empty passwords and terminates
// the renderer.
TEST_F(BadMessageTest, CheckGeneratedPassword_EmptyPassword) {
  base::HistogramTester histogram_tester;
  std::u16string empty_password;

  bool result = CheckGeneratedPassword(GetMainFrame(), empty_password);

  EXPECT_FALSE(result);
  ExpectHistogramEntry(
      histogram_tester,
      BadMessageReason::CPMD_BAD_ORIGIN_NO_GENERATED_PASSWORD_TO_EDIT);
}

// Tests that CheckGeneratedPassword() allows whitespace-only passwords.
TEST_F(BadMessageTest, CheckGeneratedPassword_WhitespaceOnlyPassword) {
  base::HistogramTester histogram_tester;
  std::u16string whitespace_password = u"   ";

  bool result = CheckGeneratedPassword(GetMainFrame(), whitespace_password);

  EXPECT_TRUE(result);
  ExpectNoHistogramEntries(histogram_tester);
}

}  // namespace password_manager::bad_message
