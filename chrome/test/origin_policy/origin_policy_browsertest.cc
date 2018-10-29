// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
const base::FilePath::CharType kDataRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/origin_policy_browsertest");

// The title of the Origin Policy error interstitial. This is used to determine
// whether the page load was blocked by the origin policy throttle.
const char kErrorInterstitialTitle[] = "Origin Policy Error Interstitial";
}  // namespace

namespace content {

// OriginPolicyBrowserTest tests several aspects of OriginPolicyThrottle (plus
// associated logic elsewhere). These tests focus on error conditions, since
// the normal operating conditions are already well covered in cross-browser
// Web Platform Tests (wpt/origin-policy/*).

class OriginPolicyBrowserTest : public InProcessBrowserTest {
 public:
  OriginPolicyBrowserTest() : InProcessBrowserTest() {}
  ~OriginPolicyBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    server_->AddDefaultHandlers(base::FilePath(kDataRoot));
    EXPECT_TRUE(server()->Start());

    feature_list_.InitAndEnableFeature(features::kOriginPolicy);
  }

  void TearDownInProcessBrowserTestFixture() override { server_.reset(); }

  net::test_server::EmbeddedTestServer* server() { return server_.get(); }

  // Most tests here are set up to use the page title to distinguish between
  // successful load or the error page. For those tests, this method implements
  // the bulk of the test logic.
  base::string16 NavigateToAndReturnTitle(const char* url) {
    EXPECT_TRUE(server());
    ui_test_utils::NavigateToURL(browser(), GURL(server()->GetURL(url)));
    base::string16 title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    return title;
  }

 private:
  std::unique_ptr<net::test_server::EmbeddedTestServer> server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, PageWithoutPolicy) {
  EXPECT_EQ(base::ASCIIToUTF16("Page Without Policy"),
            NavigateToAndReturnTitle("/page-without-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ApplyPolicy) {
  EXPECT_EQ(base::ASCIIToUTF16("Page With Policy"),
            NavigateToAndReturnTitle("/page-with-policy.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorCantDownloadPolicy) {
  EXPECT_EQ(base::ASCIIToUTF16(kErrorInterstitialTitle),
            NavigateToAndReturnTitle("/page-policy-missing.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy301Redirect) {
  EXPECT_EQ(base::ASCIIToUTF16(kErrorInterstitialTitle),
            NavigateToAndReturnTitle("/page-policy-301redirect.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy302Redirect) {
  EXPECT_EQ(base::ASCIIToUTF16(kErrorInterstitialTitle),
            NavigateToAndReturnTitle("/page-policy-302redirect.html"));
}

IN_PROC_BROWSER_TEST_F(OriginPolicyBrowserTest, ErrorPolicy307Redirect) {
  EXPECT_EQ(base::ASCIIToUTF16(kErrorInterstitialTitle),
            NavigateToAndReturnTitle("/page-policy-307redirect.html"));
}

}  // namespace content
