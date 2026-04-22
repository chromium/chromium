// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class UnloadPlatformBrowserTest : public PlatformBrowserTest {
 public:
  UnloadPlatformBrowserTest() = default;
  ~UnloadPlatformBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Tests setting values in local storage in the unload event handler. This
// behaves differently on android vs traditional desktop platforms.
// See https://crbug.com/505286903.
IN_PROC_BROWSER_TEST_F(UnloadPlatformBrowserTest, SettingValuesFromOnUnload) {
  // We need an HTTPS server. Only https sites are allowed to use the unload
  // event in Chrome now.
  net::EmbeddedTestServer::ServerCertificate server_certificate =
      net::EmbeddedTestServer::ServerCertificate::CERT_TEST_NAMES;
  net::EmbeddedTestServer https_test_server(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_test_server.SetSSLConfig(server_certificate);
  ASSERT_TRUE(https_test_server.Start());

  const GURL url_a1(https_test_server.GetURL("a.test", "/title1.html"));
  const GURL url_a2(https_test_server.GetURL("a.test", "/title2.html"));

  content::WebContents* const web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents, url_a1));
  ASSERT_EQ(url_a1, web_contents->GetLastCommittedURL());

  static constexpr char kUnloadListenerScript[] =
      R"(localStorage['unload_ran'] = 'no';
         window.addEventListener('unload', () => {
           localStorage['unload_ran'] = 'yes';
         });)";

  EXPECT_TRUE(content::ExecJs(web_contents, kUnloadListenerScript));

  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents, url_a2));
  ASSERT_EQ(url_a2, web_contents->GetLastCommittedURL());

  std::string unload_ran =
      content::EvalJs(web_contents, "localStorage['unload_ran']")
          .ExtractString();

  std::string expected_result =
#if BUILDFLAG(IS_ANDROID)
      // On Android, the unload event does not fire (or at least, does not give
      // the page a chance to do anything).
      "no";
#else
      // On other platforms, we expect it to fire.
      "yes";
#endif

  EXPECT_EQ(expected_result, unload_ran);
}
