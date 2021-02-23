// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/search/ntp_features.h"
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

cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* cart_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(cart_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

const char kMockExample[] = "walmart.com";
const char kMockExampleURL[] = "https://www.walmart.com/cart";
const cart_db::ChromeCartContentProto kMockExampleProto =
    BuildProto(kMockExample, kMockExampleURL);
const char kMockAmazon[] = "amazon.com";
const char kMockAmazonURL[] = "https://www.amazon.com/gp/cart/view.html";
const cart_db::ChromeCartContentProto kMockAmazonProto =
    BuildProto(kMockAmazon, kMockAmazonURL);

using ShoppingCarts =
    std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
const ShoppingCarts kExpectedExample = {{kMockExample, kMockExampleProto}};
const ShoppingCarts kExpectedAmazon = {{kMockAmazon, kMockAmazonProto}};
const ShoppingCarts kEmptyExpected = {};

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
    scoped_feature_list_.InitAndEnableFeature(
        ntp_features::kNtpChromeCartModule);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    service_ = CartServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

    // This is necessary to test non-localhost domains. See |NavigateToURL|.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/cart/");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&BasicResponse));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(const std::string& url) {
    // All domains resolve to 127.0.0.1 in this test.
    GURL gurl(url);
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL(gurl.host(), gurl.path())));
    base::RunLoop().RunUntilIdle();
  }

  void SendXHR(const std::string& relative_path, const char post_data[]) {
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
    std::string script =
        content::JsReplace(script_template, relative_path, post_data);
    ASSERT_EQ(true, EvalJs(web_contents(), script)) << script;
  }

  void WaitForCartCount(const ShoppingCarts& expected) {
    satisfied_ = false;
    while (true) {
      base::RunLoop().RunUntilIdle();
      base::RunLoop run_loop;
      service_->LoadAllActiveCarts(base::BindOnce(
          &CommerceHintAgentTest::CheckCartCount, base::Unretained(this),
          run_loop.QuitClosure(), expected));
      run_loop.Run();
      if (satisfied_)
        break;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  void CheckCartCount(base::OnceClosure closure,
                      ShoppingCarts expected,
                      bool result,
                      ShoppingCarts found) {
    bool same_size = found.size() == expected.size();
    satisfied_ = same_size;
    if (same_size) {
      for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(found[i].first, expected[i].first);
        EXPECT_EQ(found[i].second.merchant_cart_url(),
                  expected[i].second.merchant_cart_url());
      }
    }
    std::move(closure).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CartService* service_;
  bool satisfied_;
};

// TODO(crbug/1179241): Deflake this test.
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, DISABLED_AddToCartByURL) {
  // For add-to-cart by URL, normally a URL in that domain has already been
  // committed.
  NavigateToURL("https://www.walmart.com/");
  NavigateToURL("https://www.walmart.com/add-to-cart?product=1");

  WaitForCartCount(kExpectedExample);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm) {
  NavigateToURL("https://www.walmart.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForCartCount(kExpectedExample);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL_XHR) {
  NavigateToURL("https://www.walmart.com/");
  SendXHR("/add-to-cart", "product: 123");

  WaitForCartCount(kExpectedExample);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCart) {
  NavigateToURL("https://www.walmart.com/cart");

  WaitForCartCount(kExpectedExample);
}

// TODO(crbug/1179241): Deflake this test.
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, DISABLED_VisitCheckout) {
  service_->AddCart(kMockExample, kMockExampleProto);
  WaitForCartCount(kExpectedExample);

  NavigateToURL("https://www.walmart.com/");
  NavigateToURL("https://www.walmart.com/123/checkout/456");
  WaitForCartCount(kEmptyExpected);
}

// TODO(crbug/1179241): Deflake this test.
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, DISABLED_PurchaseByURL) {
  service_->AddCart(kMockAmazon, kMockAmazonProto);
  WaitForCartCount(kExpectedAmazon);

  NavigateToURL("http://amazon.com/");
  NavigateToURL(
      "http://amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html");
  WaitForCartCount(kEmptyExpected);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByForm) {
  service_->AddCart(kMockExample, kMockExampleProto);
  WaitForCartCount(kExpectedExample);

  NavigateToURL("https://www.walmart.com/purchase.html");

  std::string script = "document.getElementById('submit').click()";
  ASSERT_TRUE(ExecJs(web_contents(), script));
  content::TestNavigationObserver load_observer(web_contents());
  load_observer.WaitForNavigationFinished();
  WaitForCartCount(kEmptyExpected);
}

}  // namespace
