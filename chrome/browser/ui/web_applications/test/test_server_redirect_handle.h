// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_TEST_SERVER_REDIRECT_HANDLE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_TEST_SERVER_REDIRECT_HANDLE_H_

#include "base/auto_reset.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace web_app {

// Test helper for injecting temporary redirects into EmbeddedTestServer.
// Example usage:
//
//   class ExampleBrowserTest : public InProcessBrowserTest {
//    public:
//     ExampleBrowserTest() : redirect_handle_(*embedded_test_server()) {}
//
//     void SetUpOnMainThread() override {
//       ASSERT_TRUE(embedded_test_server()->Start());
//       InProcessBrowserTest::SetUpOnMainThread();
//     }
//
//     void Navigate(const GURL& url) { ... }
//     GURL CurrentUrl() { ... }
//
//    private:
//     TestServerRedirectHandle redirect_handle_;
//   };
//
//   IN_PROC_BROWSER_TEST_P(ExampleBrowserTest, RedirectTest) {
//     auto redirect_scope = redirect_handle_.Redirect({
//         .redirect_url = embedded_test_server()->GetURL("app.com", "/redir"),
//         .target_url = embedded_test_server()->GetURL("app.com", "/target"),
//         .origin = "app.com",
//     });
//     Navigate(redirect_handle_.params().redirect_url);
//     EXPECT_EQ(CurrentUrl(), redirect_handle_.params().target_url);
//   }
class TestServerRedirectHandle {
 public:
  struct Params {
    GURL redirect_url;
    GURL target_url;
    const char* origin = nullptr;
    net::HttpStatusCode redirect_code = net::HTTP_TEMPORARY_REDIRECT;
  };

  explicit TestServerRedirectHandle(net::EmbeddedTestServer& test_server);

  const Params& params() const { return params_; }

  base::AutoReset<Params> Redirect(Params params);

 private:
  Params params_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_TEST_SERVER_REDIRECT_HANDLE_H_
