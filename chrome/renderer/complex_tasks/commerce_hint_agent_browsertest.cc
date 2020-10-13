// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  // This should be served from test data.
  if (request.relative_url == "/purchase.html")
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("dummy");
  response->set_content_type("text/html");
  return response;
}

// Tests CommerceHintAgent.
class CommerceHintAgentTest : public PlatformBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCommerceHint);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/complex_tasks/");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&BasicResponse));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToPath(const std::string& path) {
    ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                       embedded_test_server()->GetURL(path)));
  }

  void NavigateToURL(const std::string& url) {
    GURL gurl(url);
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL(gurl.host(), gurl.path())));
  }

  void SendXHR(const std::string& path, const char post_data[]) {
    GURL url = embedded_test_server()->GetURL(path);
    const char script_template[] = R"(
        new Promise(function (resolve, reject) {
          let xhr = new XMLHttpRequest();
          xhr.open('POST', $1, true);
          xhr.onload = () => {
            resolve(true);
          };
          xhr.send($2);
        });
    )";
    std::string script = content::JsReplace(script_template, url, post_data);
    ASSERT_EQ(true, EvalJs(web_contents(), script)) << script;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL) {
  NavigateToPath("/add-to-cart?product=1");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 0);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm) {
  NavigateToPath("/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 0);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCart) {
  NavigateToPath("/cart");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 0);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCheckout) {
  NavigateToPath("/123/checkout/456");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 0);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByURL) {
  NavigateToURL(
      "http://amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 1);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByForm) {
  NavigateToPath("/purchase.html");

  std::string script = "document.getElementById('submit').click()";
  ASSERT_TRUE(ExecJs(web_contents(), script));
  content::TestNavigationObserver load_observer(web_contents());
  load_observer.WaitForNavigationFinished();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.AddToCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCart"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.VisitCheckout"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Commerce.Purchase"), 1);
}

}  // namespace
