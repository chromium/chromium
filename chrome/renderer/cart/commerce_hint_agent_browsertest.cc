// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"

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

cart_db::ChromeCartProductProto BuildProductInfoProto(const char* product_id) {
  cart_db::ChromeCartProductProto proto;
  proto.set_product_id(product_id);
  return proto;
}

cart_db::ChromeCartContentProto BuildProtoWithProducts(
    const char* domain,
    const char* cart_url,
    const std::vector<const char*>& product_urls) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(cart_url);
  for (const auto* const v : product_urls) {
    proto.add_product_image_urls(v);
  }
  return proto;
}

cart_db::ChromeCartContentProto BuildProtoWithProducts(
    const char* domain,
    const char* cart_url,
    const std::vector<const char*>& product_urls,
    const std::vector<const char*>& product_ids) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(cart_url);
  for (const auto* const url : product_urls) {
    proto.add_product_image_urls(url);
  }
  for (const auto* const id : product_ids) {
    auto* added_product = proto.add_product_infos();
    *added_product = BuildProductInfoProto(id);
  }
  return proto;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

const char kMockExample[] = "guitarcenter.com";
const char kMockExampleFallbackURL[] = "https://www.guitarcenter.com/cart";
const char kMockExampleLinkURL[] =
    "https://www.guitarcenter.com/shopping-cart/";
const char kMockExampleURL[] = "https://www.guitarcenter.com/cart.html";
const char kMockExampleURL2[] =
    "https://www.guitarcenter.com/shopping-cart.html";

const cart_db::ChromeCartContentProto kMockExampleProtoFallbackCart =
    BuildProto(kMockExample, kMockExampleFallbackURL);
const cart_db::ChromeCartContentProto kMockExampleProtoLinkCart =
    BuildProto(kMockExample, kMockExampleLinkURL);
const cart_db::ChromeCartContentProto kMockExampleProto =
    BuildProto(kMockExample, kMockExampleURL);
const cart_db::ChromeCartContentProto kMockExampleProtoWithProducts =
    BuildProtoWithProducts(
        kMockExample,
        kMockExampleURL,
        {"https://static.guitarcenter.com/product-image/foo_123-0-medium",
         "https://images.cymax.com/Images/3/bar_456-baz_789-0-medium",
         "https://static.guitarcenter.com/product-image/qux_357-0-medium"});
const cart_db::ChromeCartContentProto
    kMockExampleProtoWithProductsWithoutSaved = BuildProtoWithProducts(
        kMockExample,
        kMockExampleURL,
        {"https://static.guitarcenter.com/product-image/foo_123-0-medium",
         "https://images.cymax.com/Images/3/bar_456-baz_789-0-medium"});

const char kMockAmazon[] = "amazon.com";
const char kMockAmazonURL[] = "https://www.amazon.com/gp/cart/view.html";
const cart_db::ChromeCartContentProto kMockAmazonProto =
    BuildProto(kMockAmazon, kMockAmazonURL);

using ShoppingCarts =
    std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
const ShoppingCarts kExpectedExampleFallbackCart = {
    {kMockExample, kMockExampleProtoFallbackCart}};
const ShoppingCarts kExpectedExampleLinkCart = {
    {kMockExample, kMockExampleProtoLinkCart}};
const ShoppingCarts kExpectedExample = {{kMockExample, kMockExampleProto}};
const ShoppingCarts kExpectedExampleWithProducts = {
    {kMockExample, kMockExampleProtoWithProducts}};
const ShoppingCarts kExpectedExampleWithProductsWithoutSaved = {
    {kMockExample, kMockExampleProtoWithProductsWithoutSaved}};
const ShoppingCarts kExpectedAmazon = {{kMockAmazon, kMockAmazonProto}};
const ShoppingCarts kEmptyExpected = {};

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  // This should be served from test data.
  if (request.relative_url == "/purchase.html")
    return nullptr;
  if (request.relative_url == "/product.html")
    return nullptr;
  if (request.relative_url == "/cart.html")
    return nullptr;
  if (request.relative_url == "/shopping-cart.html")
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("dummy");
  response->set_content_type("text/html");
  return response;
}

// Tests CommerceHintAgent.
class CommerceHintAgentTest : public PlatformBrowserTest {
 public:
  using Entry = ukm::builders::Shopping_FormSubmitted;

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"product-skip-pattern", "(^|\\W)(?i)(skipped)(\\W|$)"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    service_ = CartServiceFactory::GetForProfile(profile);
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    ASSERT_TRUE(identity_manager);
    signin::SetPrimaryAccount(identity_manager, "user@gmail.com",
                              signin::ConsentLevel::kSync);

    // This is necessary to test non-localhost domains. See |NavigateToURL|.
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/cart/");
    https_server_.RegisterRequestHandler(base::BindRepeating(&BasicResponse));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(const std::string& url) {
    // All domains resolve to 127.0.0.1 in this test.
    GURL gurl(url);
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(), https_server_.GetURL(gurl.host(), gurl.path())));
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
        GURL::Replacements remove_port;
        remove_port.ClearPort();
        EXPECT_EQ(GURL(found[i].second.merchant_cart_url())
                      .ReplaceComponents(remove_port)
                      .spec(),
                  expected[i].second.merchant_cart_url());
      }
    } else {
      VLOG(3) << "Found " << found.size() << " but expecting "
              << expected.size();
    }
    std::move(closure).Run();
  }

  void WaitForCarts(const ShoppingCarts& expected) {
    satisfied_ = false;
    while (true) {
      base::RunLoop().RunUntilIdle();
      base::RunLoop run_loop;
      service_->LoadAllActiveCarts(base::BindOnce(
          &CommerceHintAgentTest::CheckCarts, base::Unretained(this),
          run_loop.QuitClosure(), expected));
      run_loop.Run();
      if (satisfied_)
        break;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  void CheckCarts(base::OnceClosure closure,
                  ShoppingCarts expected,
                  bool result,
                  ShoppingCarts found) {
    bool same_size = found.size() == expected.size();
    satisfied_ = same_size;
    if (same_size) {
      for (size_t i = 0; i < expected.size(); i++) {
        satisfied_ &= found[i].first == expected[i].first;
        GURL::Replacements remove_port;
        remove_port.ClearPort();
        satisfied_ &= GURL(found[i].second.merchant_cart_url())
                          .ReplaceComponents(remove_port)
                          .spec() == expected[i].second.merchant_cart_url();
      }
    }
    std::move(closure).Run();
  }

  void WaitForProductCount(const ShoppingCarts& expected) {
    satisfied_ = false;
    while (true) {
      base::RunLoop().RunUntilIdle();
      base::RunLoop run_loop;
      service_->LoadAllActiveCarts(base::BindOnce(
          &CommerceHintAgentTest::CheckCartWithProducts, base::Unretained(this),
          run_loop.QuitClosure(), expected));
      run_loop.Run();
      if (satisfied_)
        break;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  void CheckCartWithProducts(base::OnceClosure closure,
                             ShoppingCarts expected,
                             bool result,
                             ShoppingCarts found) {
    bool fail = false;
    bool same_size = found.size() == expected.size();
    if (!same_size)
      fail = true;
    for (size_t i = 0; i < std::min(found.size(), expected.size()); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      GURL::Replacements remove_port;
      remove_port.ClearPort();
      EXPECT_EQ(GURL(found[i].second.merchant_cart_url())
                    .ReplaceComponents(remove_port)
                    .spec(),
                expected[i].second.merchant_cart_url());
      bool same_size = found[i].second.product_image_urls_size() ==
                       expected[i].second.product_image_urls_size();
      if (!same_size) {
        fail = true;
      } else {
        for (int j = 0; j < found[i].second.product_image_urls_size(); j++) {
          EXPECT_EQ(found[i].second.product_image_urls(j),
                    expected[i].second.product_image_urls(j));
        }
      }
      same_size = found[i].second.product_infos_size() ==
                  expected[i].second.product_infos_size();
      if (!same_size) {
        fail = true;
      } else {
        for (int j = 0; j < found[i].second.product_infos_size(); j++) {
          EXPECT_EQ(found[i].second.product_infos(j).product_id(),
                    expected[i].second.product_infos(j).product_id());
        }
      }
    }
    satisfied_ = !fail;
    std::move(closure).Run();
  }

  void ExpectUKM(const std::string& metric_name) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM \"" << metric_name << "\" was not recorded";
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  CartService* service_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  bool satisfied_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL) {
  // For add-to-cart by URL, normally a URL in that domain has already been
  // committed.
  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");

  WaitForCartCount(kExpectedExampleFallbackCart);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForCartCount(kExpectedExampleFallbackCart);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm_WithLink) {
  NavigateToURL("https://www.guitarcenter.com/product.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForCartCount(kExpectedExampleLinkCart);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm_WithWrongLink) {
  // Mismatching eTLD+1 domain uses cart URL in the look-up table.
  NavigateToURL("https://amazon.com/product.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForCartCount(kExpectedAmazon);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL_XHR) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");

  WaitForCartCount(kExpectedExampleFallbackCart);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCart) {
  // Cannot use dummy page with zero products, or the cart would be deleted.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForCartCount(kExpectedExample);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, ExtractCart) {
  // This page has three products.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForProductCount(kExpectedExampleWithProducts);
}

class CommerceHintNoRateControlTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"cart-extraction-gap-time", "0s"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1241582): Add the rate control back for this test after
// figuring out why rate control makes this test flaky.
IN_PROC_BROWSER_TEST_F(CommerceHintNoRateControlTest, CartPriority) {
  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForCarts(kExpectedExample);

  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  WaitForCarts(kExpectedExample);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCheckout) {
  service_->AddCart(kMockExample, absl::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/123/checkout/456");
  WaitForCartCount(kEmptyExpected);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByURL) {
  service_->AddCart(kMockAmazon, absl::nullopt, kMockAmazonProto);
  WaitForCartCount(kExpectedAmazon);

  NavigateToURL("http://amazon.com/");
  NavigateToURL(
      "http://amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html");
  WaitForCartCount(kEmptyExpected);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByForm) {
  service_->AddCart(kMockExample, absl::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/purchase.html");

  std::string script = "document.getElementById('submit').click()";
  ASSERT_TRUE(ExecJs(web_contents(), script));
  content::TestNavigationObserver load_observer(web_contents());
  load_observer.WaitForNavigationFinished();
  WaitForCartCount(kEmptyExpected);
  ExpectUKM("IsTransaction");
}

// TODO(crbug.com/1180268): CrOS multi-profiles implementation is different from
// the rest and below tests don't work on CrOS yet. Re-enable them on CrOS after
// figuring out the reason for failure.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, NonSignInUser) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ASSERT_TRUE(identity_manager);
  signin::ClearPrimaryAccount(identity_manager);
  NavigateToURL("https://www.guitarcenter.com/cart");
  WaitForCartCount(kEmptyExpected);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  WaitForCartCount(kEmptyExpected);

  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kEmptyExpected);

  signin::SetPrimaryAccount(identity_manager, "user@gmail.com",
                            signin::ConsentLevel::kSync);
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kExpectedExampleFallbackCart);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, MultipleProfiles) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(identity_manager);
  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 1U);
  signin::ClearPrimaryAccount(identity_manager);
  NavigateToURL("https://www.guitarcenter.com/cart");
  WaitForCartCount(kEmptyExpected);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  WaitForCartCount(kEmptyExpected);

  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kEmptyExpected);

  // Create another profile.
  base::FilePath profile_path2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      profile_path2, base::BindRepeating(&UnblockOnProfileCreation, &run_loop));
  run_loop.Run();
  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 2U);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kExpectedExampleFallbackCart);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class CommerceHintCacaoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kNtpChromeCartModule,
         optimization_guide::features::kOptimizationHints},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintCacaoTest, Passed) {
  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  // Need the non-default port here.
  optimization_guide_decider->AddHintForTesting(
      https_server_.GetURL("www.guitarcenter.com", "/"),
      optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, absl::nullopt);
  optimization_guide_decider->AddHintForTesting(
      GURL("https://www.guitarcenter.com/cart"),
      optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, absl::nullopt);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kExpectedExampleFallbackCart);
}

// If command line argument "optimization_guide_hints_override" is not given,
// nothing is specified in AddHintForTesting(), and the real hints are not
// downloaded, all the URLs are considered non-shopping.
IN_PROC_BROWSER_TEST_F(CommerceHintCacaoTest, Rejected) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 30);
  WaitForCartCount(kEmptyExpected);
}

class CommerceHintProductInfoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
            "true"},
           {"partner-merchant-pattern", "(guitarcenter.com)"},
           {"product-skip-pattern", "(^|\\W)(?i)(skipped)(\\W|$)"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest, AddToCartByForm_CaptureId) {
  NavigateToURL("https://www.guitarcenter.com/product.html");
  SendXHR("/cart/update", "product_id=id_foo&add_to_cart=true");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(kMockExample, kMockExampleLinkURL, {}, {"id_foo"});
  const ShoppingCarts expected_carts = {{kMockExample, expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest, AddToCartByURL_CaptureId) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart?pr1id=id_foo", "");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(kMockExample, kMockExampleFallbackURL, {},
                             {"id_foo"});
  const ShoppingCarts expected_carts = {{kMockExample, expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_FromURL) {
  // This page has two products.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          kMockExample, kMockExampleURL,
          {"https://static.guitarcenter.com/product-image/foo_123-0-medium",
           "https://images.cymax.com/Images/3/bar_456-baz_789-0-medium",
           "https://static.guitarcenter.com/product-image/qux_357-0-medium"},
          {"foo_123", "bar_456", "qux_357"});
  const ShoppingCarts expected_carts = {{kMockExample, expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_FromElement) {
  // This page has two products.
  NavigateToURL("https://www.guitarcenter.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          kMockExample, kMockExampleURL2,
          {"https://static.guitarcenter.com/product-image/foo",
           "https://static.guitarcenter.com/product-image/bar"},
          {"foo_123", "bar_456"});
  const ShoppingCarts expected_carts = {{kMockExample, expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       RBDPartnerCartURLNotOverwrite) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);
  EXPECT_TRUE(service_->IsCartDiscountEnabled());

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");

  WaitForCartCount(kExpectedExampleFallbackCart);
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForCartCount(kExpectedExampleFallbackCart);
}

class CommerceHintImprovementTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam,
            "true"},
           {"product-skip-pattern", "(^|\\W)(?i)(skipped)(\\W|$)"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintImprovementTest, ExtractCart) {
  // This page has three products but should ignore the one in saved for later
  // section.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForProductCount(kExpectedExampleWithProductsWithoutSaved);
}

class CommerceHintSkippAddToCartTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"skip-add-to-cart-mapping", R"({"guitarcenter.com": ".*"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintSkippAddToCartTest, AddToCartByForm) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForCartCount(kEmptyExpected);

  // Test AddToCart that is supposed to be skipped based on resources is now
  // overwritten.
  const cart_db::ChromeCartContentProto qvc_cart =
      BuildProto("qvc.com", "https://www.qvc.com/checkout/cart.html");
  const ShoppingCarts result = {{"qvc.com", qvc_cart}};
  NavigateToURL("https://www.qvc.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  WaitForCartCount(result);
}

class CommerceHintCheckoutPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"checkout-pattern-mapping",
            R"({"guitarcenter.com": "special_checkout_text"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintCheckoutPatternTest, VisitCheckout) {
  service_->AddCart(kMockExample, absl::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/special_checkout_text");
  WaitForCartCount(kEmptyExpected);
}

class CommerceHintPurchaseButtonPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"purchase-button-pattern-mapping",
            R"({"guitarcenter.com": "special text"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintPurchaseButtonPatternTest, PurchaseByForm) {
  service_->AddCart(kMockExample, absl::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/purchase.html");

  std::string script = "document.getElementById('special-submit').click()";
  ASSERT_TRUE(ExecJs(web_contents(), script));
  content::TestNavigationObserver load_observer(web_contents());
  load_observer.WaitForNavigationFinished();
  WaitForCartCount(kEmptyExpected);
}

class CommerceHintPurchaseURLPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{"purchase-url-pattern-mapping",
            R"({"guitarcenter.com": "special_purchase_text"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintPurchaseURLPatternTest, PurchaseByURL) {
  service_->AddCart(kMockExample, absl::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/special_purchase_text");
  WaitForCartCount(kEmptyExpected);
}
}  // namespace
