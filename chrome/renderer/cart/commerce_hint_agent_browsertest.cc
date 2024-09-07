// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cart/commerce_hint_service.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/session_proto_db/session_proto_db.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* cart_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant(domain);
  proto.set_merchant_cart_url(cart_url);
  proto.set_timestamp(base::Time::Now().InSecondsFSinceUnixEpoch());
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

const char kMockExample[] = "guitarcenter.com";
const char kMockExampleFallbackURL[] = "https://www.guitarcenter.com/cart";
const char kMockExampleLinkURL[] =
    "https://www.guitarcenter.com/shopping-cart/";
const char kMockExampleURL[] = "https://www.guitarcenter.com/cart.html";

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
        {"https://static.guitarcenter.com/product-image/foo_123",
         "https://images.cymax.com/Images/3/bar_456-baz_789",
         "https://static.guitarcenter.com/product-image/qux_357"});
const cart_db::ChromeCartContentProto
    kMockExampleProtoWithProductsWithoutSaved = BuildProtoWithProducts(
        kMockExample,
        kMockExampleURL,
        {"https://static.guitarcenter.com/product-image/foo_123",
         "https://images.cymax.com/Images/3/bar_456-baz_789"});

const char kMockAmazon[] = "amazon.com";
const char kMockAmazonURL[] = "https://www.amazon.com/gp/cart/view.html";
const cart_db::ChromeCartContentProto kMockAmazonProto =
    BuildProto(kMockAmazon, kMockAmazonURL);

const char kMockWalmart[] = "walmart.com";
const char kMockWalmartURL[] = "https://www.walmart.com/cart";
const cart_db::ChromeCartContentProto kMockWalmartProto =
    BuildProto(kMockWalmart, kMockWalmartURL);

using ShoppingCarts =
    std::vector<SessionProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
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
const ShoppingCarts kExpectedWalmart = {{kMockWalmart, kMockWalmartProto}};
const ShoppingCarts kEmptyExpected = {};
#endif

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
  if (request.relative_url == "/product-page.html")
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("dummy");
  response->set_content_type("text/html");
  return response;
}

// Tests CommerceHintAgent.
class CommerceHintAgentTest : public PlatformBrowserTest {
 public:
  using FormSubmittedEntry = ukm::builders::Shopping_FormSubmitted;
  using XHREntry = ukm::builders::Shopping_WillSendRequest;
  using ExtractionEntry = ukm::builders::Shopping_CartExtraction;
  using AddToCartEntry = ukm::builders::Shopping_AddToCartDetection;

  CommerceHintAgentTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"product-skip-pattern", "(^|\\W)(?i)(skipped)(\\W|$)"},
             // Extend timeout to avoid flakiness.
             {"cart-extraction-timeout", "1m"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // TODO(crbug.com/40285326): This fails with the field trial testing config.
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
#if !BUILDFLAG(IS_ANDROID)
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    service_ = CartServiceFactory::GetForProfile(profile);
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    ASSERT_TRUE(identity_manager);
    signin::SetPrimaryAccount(identity_manager, "user@gmail.com",
                              signin::ConsentLevel::kSync);
#endif

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

#if !BUILDFLAG(IS_ANDROID)
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
        satisfied_ &=
            found[i].second.merchant() == expected[i].second.merchant();
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
      same_size = found[i].second.product_image_urls_size() ==
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
#endif

  void ExpectUKMCount(std::string_view entry_name,
                      const std::string& metric_name,
                      int expected_count) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);
    int count = 0;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        count += 1;
      }
    }
    EXPECT_EQ(count, expected_count);
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void WaitForUmaCount(std::string_view name,
                       base::HistogramBase::Count expected_count) {
    while (true) {
      base::RunLoop().RunUntilIdle();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
      base::HistogramBase::Count count = 0;
      for (const auto& bucket : histogram_tester_.GetAllSamples(name))
        count += bucket.count;
      ASSERT_LE(count, expected_count) << "WaitForUmaCount(" << name
                                       << ") has more counts than expectation.";
      if (count == expected_count)
        break;
      LOG(INFO) << "WaitForUmaCount() is expecting " << expected_count
                << " but found " << count;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 10);
    }
  }

  void WaitForUmaBucketCount(std::string_view name,
                             base::HistogramBase::Sample sample,
                             base::HistogramBase::Count expected_count) {
    while (true) {
      base::RunLoop().RunUntilIdle();
      metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
      auto count = histogram_tester_.GetBucketCount(name, sample);
      if (count == expected_count)
        break;
      LOG(INFO) << "WaitForUmaBucketCount() is expecting " << expected_count
                << " but found " << count;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 10);
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<CartService, DanglingUntriaged> service_;
#endif
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  bool satisfied_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL) {
  // For add-to-cart by URL, normally a URL in that domain has already been
  // committed.
  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");

  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  ExpectUKMCount(XHREntry::kEntryName, "IsAddToCart", 1);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm_WithLink) {
  NavigateToURL("https://www.guitarcenter.com/product.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleLinkCart);
#endif
  ExpectUKMCount(XHREntry::kEntryName, "IsAddToCart", 1);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByForm_WithWrongLink) {
  // Mismatching eTLD+1 domain uses cart URL in the look-up table.
  NavigateToURL("https://walmart.com/product.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
  WaitForCartCount(kExpectedWalmart);
  ExpectUKMCount(XHREntry::kEntryName, "IsAddToCart", 1);
}
#endif

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddToCartByURL_XHR) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");

  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  ExpectUKMCount(XHREntry::kEntryName, "IsAddToCart", 1);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, SkipAddToCart_FromComponent) {
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), R"###(
          {
            "guitarcenter.com": {
              "skip_add_to_cart_regex": "dummy-request"
            }
          }
      )###",
          "{}", "", "");
  DCHECK(is_populated);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);

  SendXHR("/add-to-cart/dummy-request-url", "product: 123");
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);
}

// TODO(https://crbug/1310497, https://crbug.com/1362442): This test is flaky
// on ChromeOS and Linux Asan.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_VisitCart DISABLED_VisitCart
#elif BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_VisitCart DISABLED_VisitCart
#else
#define MAYBE_VisitCart VisitCart
#endif

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, MAYBE_VisitCart) {
  // Cannot use dummy page with zero products, or the cart would be deleted.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForUmaCount("Commerce.Carts.VisitCart", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExample);
#endif
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest,
                       VisitCart_GeneralPattern_FromComponent) {
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), "{}", R"###(
          {
            "cart_page_url_regex": "(special|lol)"
          }
      )###",
          "", "");
  DCHECK(is_populated);

  NavigateToURL("https://www.guitarcenter.com/special.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/lol.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 2);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest,
                       VisitCart_PerDomain_FromComponent) {
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), R"###(
          {
            "guitarcenter.com": {
              "cart_url_regex" : "unique|laugh"
            }
          }
      )###",
          R"###(
          {
            "cart_page_url_regex": "(special|lol)"
          }
      )###",
          "", "");
  DCHECK(is_populated);

  NavigateToURL("https://www.guitarcenter.com/unique.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/special.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/laugh.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 2);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, ExtractCart_ScriptFromResource) {
  // This page has three products.
  NavigateToURL("https://www.guitarcenter.com/cart.html");
#if !BUILDFLAG(IS_ANDROID)
  WaitForProductCount(kExpectedExampleWithProductsWithoutSaved);
#endif
  WaitForUmaCount("Commerce.Carts.ExtractionExecutionTime", 1);
  WaitForUmaCount("Commerce.Carts.ExtractionLongestTaskTime", 1);
  WaitForUmaCount("Commerce.Carts.ExtractionTotalTasksTime", 1);
  WaitForUmaCount("Commerce.Carts.ExtractionElapsedTime", 1);
  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 1);
  WaitForUmaBucketCount(
      "Commerce.Heuristics.CartExtractionScriptSource",
      int(CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_RESOURCE),
      1);
  WaitForUmaBucketCount("Commerce.Heuristics.ProductIDExtractionPatternSource",
                        int(CommerceHeuristicsDataMetricsHelper::
                                HeuristicsSource::FROM_FEATURE_PARAMETER),
                        1);
  ExpectUKMCount(ExtractionEntry::kEntryName, "ExtractionExecutionTime", 1);
  ExpectUKMCount(ExtractionEntry::kEntryName, "ExtractionLongestTaskTime", 1);
  ExpectUKMCount(ExtractionEntry::kEntryName, "ExtractionTotalTasksTime", 1);
  ExpectUKMCount(ExtractionEntry::kEntryName, "ExtractionElapsedTime", 1);
  ExpectUKMCount(ExtractionEntry::kEntryName, "ExtractionTimedOut", 1);

  SendXHR("/add-to-cart", "product: 123");

  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 2);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, ExtractCart_ScriptFromComponent) {
  // Initialize component with a dummy script that returns immediately.
  std::string extraction_script = R"###(
    async function extractAllItems(root) {
      return {
        "products":[
          {
            "imageUrl": "https://foo.com/bar/image",
            "price": "$10",
            "title": "Foo bar",
            "url": "https://foo.com/bar",
          }
        ]
      };
    }
    extracted_results_promise = extractAllItems(document);
  )###";
  std::string product_id_json = "{\"foo.com\": \"test\"}";
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), "{}", "{}", std::move(product_id_json),
          std::move(extraction_script));
  DCHECK(is_populated);

  NavigateToURL("https://www.guitarcenter.com/cart.html");

#if !BUILDFLAG(IS_ANDROID)
  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts("guitarcenter.com",
                             "https://www.guitarcenter.com/cart.html",
                             {"https://foo.com/bar/image"});
  const ShoppingCarts expected_carts = {
      {"guitarcenter.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
#endif
  WaitForUmaBucketCount("Commerce.Heuristics.CartExtractionScriptSource",
                        int(CommerceHeuristicsDataMetricsHelper::
                                HeuristicsSource::FROM_COMPONENT),
                        1);
  WaitForUmaBucketCount("Commerce.Heuristics.ProductIDExtractionPatternSource",
                        int(CommerceHeuristicsDataMetricsHelper::
                                HeuristicsSource::FROM_COMPONENT),
                        1);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest,
                       ExtractCart_ProductIDFromComponent) {
  std::string global_heuristics = R"###(
    {
      "rule_discount_partner_merchant_regex": "(guitarcenter.com)"
    }
  )###";
  std::string product_id_json = R"###(
    {
      "product_element": {"www.guitarcenter.com": "<a href=\"#modal-(\\w+)"}
    }
  )###";
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), "{}", global_heuristics,
          std::move(product_id_json), "");
  DCHECK(is_populated);

  // This page has two products.
  NavigateToURL("https://www.guitarcenter.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "aaa.com", "https://www.guitarcenter.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_1", "bar_1"});
  const ShoppingCarts expected_carts = {
      {"guitarcenter.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
  WaitForUmaBucketCount(
      "Commerce.Heuristics.CartExtractionScriptSource",
      int(CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_RESOURCE),
      1);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, AddCartFromComponent) {
  bool is_populated =
      cart::CommerceHintService::InitializeCommerceHeuristicsForTesting(
          base::Version("0.0.0.1"), R"###(
          {
            "guitarcenter.com": {
              "merchant_name" : "SPECIAL_NAME",
              "cart_url" : "https://www.guitarcenter.com/special_cart"
            }
          }
      )###",
          "{}", "", "");
  DCHECK(is_populated);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");

  // Browser-side commerce heuristics are still correct despite being
  // used to populate renderer side commerce heuristics.
  cart_db::ChromeCartContentProto expected_cart_protos = BuildProto(
      "guitarcenter.com", "https://www.guitarcenter.com/special_cart");
  expected_cart_protos.set_merchant("SPECIAL_NAME");
  const ShoppingCarts expected_carts = {
      {"guitarcenter.com", expected_cart_protos}};
  WaitForCarts(expected_carts);
}

class CommerceHintNoRateControlTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"cart-extraction-gap-time", "0s"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40194728): Add the rate control back for this test after
// figuring out why rate control makes this test flaky.
// Disabled due to failing tests. https://crbug.com/1254802
IN_PROC_BROWSER_TEST_F(CommerceHintNoRateControlTest, DISABLED_CartPriority) {
  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  WaitForCartCount(kExpectedExampleFallbackCart);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForCarts(kExpectedExample);

  NavigateToURL("https://www.guitarcenter.com/");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  WaitForCarts(kExpectedExample);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, VisitCheckout) {
  GURL example_url = GURL("https://www.guitarcenter.com/");
#if !BUILDFLAG(IS_ANDROID)
  service_->AddCart(example_url, std::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif

  NavigateToURL(example_url.spec());
  NavigateToURL("https://www.guitarcenter.com/123/checkout/456");
  // URL is checked against checkout twice.
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 2);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByURL) {
  GURL amazon_url = GURL("https://www.amazon.com/");
#if !BUILDFLAG(IS_ANDROID)
  service_->AddCart(amazon_url, std::nullopt, kMockAmazonProto);
  WaitForCartCount(kExpectedAmazon);
#endif

  NavigateToURL(amazon_url.spec());
  NavigateToURL(
      "http://amazon.com/gp/buy/spc/handlers/static-submit-decoupled.html");
  WaitForUmaCount("Commerce.Carts.PurchaseByURL", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
}

IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, PurchaseByForm) {
  GURL example_url = GURL("https://www.guitarcenter.com/");
#if !BUILDFLAG(IS_ANDROID)
  service_->AddCart(example_url, std::nullopt, kMockExampleProto);
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif

  NavigateToURL("https://www.guitarcenter.com/purchase.html");

  std::string script = "document.getElementById('submit').click()";
  ASSERT_TRUE(ExecJs(web_contents(), script));
  content::TestNavigationObserver load_observer(web_contents());
  load_observer.WaitForNavigationFinished();
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  ExpectUKMCount(FormSubmittedEntry::kEntryName, "IsTransaction", 1);
}

// TODO(crbug.com/40169871): CrOS multi-profiles implementation is different
// from the rest and below tests don't work on CrOS yet. Re-enable them on CrOS
// after figuring out the reason for failure. Signing out on Lacros is not
// possible.
// TODO(crbug.com/40227790): Intentionally skip below two tests for Android for
// now.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40201179): Skip work on non-eligible profiles.
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

// TODO(crbug.com/40201179): Skip work on non-eligible profiles.
// Flaky on Linux Asan and Mac: https://crbug.com/1306908.
#if (BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)) || BUILDFLAG(IS_MAC)
#define MAYBE_MultipleProfiles DISABLED_MultipleProfiles
#else
#define MAYBE_MultipleProfiles MultipleProfiles
#endif
IN_PROC_BROWSER_TEST_F(CommerceHintAgentTest, MAYBE_MultipleProfiles) {
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
  profiles::testing::CreateProfileSync(profile_manager, profile_path2);
  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 2U);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
  WaitForCartCount(kExpectedExampleFallbackCart);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

class CommerceHintCacaoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {
#if !BUILDFLAG(IS_ANDROID)
          ntp_features::kNtpChromeCartModule,
#else
          commerce::kCommerceHintAndroid,
#endif
              optimization_guide::features::kOptimizationHints
        },
        {commerce::kChromeCartDomBasedHeuristics});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintCacaoTest, Passed) {
  auto* optimization_guide_decider =
#if !BUILDFLAG(IS_ANDROID)
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
#else
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this));
#endif
  // Need the non-default port here.
  optimization_guide_decider->AddHintForTesting(
      https_server_.GetURL("www.guitarcenter.com", "/"),
      optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, std::nullopt);
  optimization_guide_decider->AddHintForTesting(
      GURL("https://www.guitarcenter.com/cart"),
      optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, std::nullopt);

  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/add-to-cart", "product: 123");
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);
}

// If command line argument "optimization_guide_hints_override" is not given,
// nothing is specified in AddHintForTesting(), and the real hints are not
// downloaded, all the URLs are considered non-shopping.
IN_PROC_BROWSER_TEST_F(CommerceHintCacaoTest, Rejected) {
  NavigateToURL("https://www.guitarcenter.com/cart");
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif

  SendXHR("/add-to-cart", "product: 123");
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 30);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 0);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.VisitCart", 0);
}

#if !BUILDFLAG(IS_ANDROID)
class CommerceHintProductInfoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpChromeCartModule,
          {{ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam,
            "true"},
           {"partner-merchant-pattern",
            "(guitarcenter.com|aaa.com|bbb.com|ccc.com|ddd.com)"},
           {"product-skip-pattern", "(^|\\W)(?i)(skipped)(\\W|$)"},
           {"product-id-pattern-mapping",
            R"###(
              {
                "product_element":
                {"www.aaa.com": "<a href=\"#modal-(\\w+)"},
                "product_image_url":
                {"www.bbb.com": "(\\w+)-\\d+-medium",
                 "www.ddd.com": ["(\\w+)-\\d+-medium", 0]
                },
                "product_url":
                {"www.ccc.com": "products-(\\w+)",
                 "www.guitarcenter.com": "products-(\\w+)"}
              }
            )###"},
           // Extend timeout to avoid flakiness.
           {"cart-extraction-timeout", "1m"}}},
         {commerce::kRetailCoupons,
          {{"coupon-partner-merchant-pattern", "(eee.com)"},
           {"coupon-product-id-pattern-mapping",
            R"###(
              {"product_url": {"www.eee.com": "products-(\\w+)"}}
            )###"}}}},
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
                       ExtractCart_CaptureId_FromElement) {
  // This page has two products.
  NavigateToURL("https://www.aaa.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "aaa.com", "https://www.aaa.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_1", "bar_1"});
  const ShoppingCarts expected_carts = {{"aaa.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_FromImageURL) {
  // This page has two products.
  NavigateToURL("https://www.bbb.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "bbb.com", "https://www.bbb.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_2", "bar_2"});
  const ShoppingCarts expected_carts = {{"bbb.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_FromProductURL) {
  // This page has two products.
  NavigateToURL("https://www.ccc.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "ccc.com", "https://www.ccc.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_3", "bar_3"});
  const ShoppingCarts expected_carts = {{"ccc.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_CaptureGroupIndex) {
  // This page has two products.
  NavigateToURL("https://www.ddd.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "ddd.com", "https://www.ddd.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_2-0-medium", "bar_2-0-medium"});
  const ShoppingCarts expected_carts = {{"ddd.com", expected_cart_protos}};
  WaitForProductCount(expected_carts);
}

IN_PROC_BROWSER_TEST_F(CommerceHintProductInfoTest,
                       ExtractCart_CaptureId_CouponPartnerMerchants) {
  // This page has two products.
  NavigateToURL("https://www.eee.com/shopping-cart.html");

  const cart_db::ChromeCartContentProto expected_cart_protos =
      BuildProtoWithProducts(
          "eee.com", "https://www.eee.com/shopping-cart.html",
          {"https://static.guitarcenter.com/product-image/foo_2-0-medium",
           "https://static.guitarcenter.com/product-image/bar_2-0-medium"},
          {"foo_3", "bar_3"});
  const ShoppingCarts expected_carts = {{"eee.com", expected_cart_protos}};
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
#endif  // !BUILDFLAG(IS_ANDROID)

// Product extraction would always timeout and return empty results.
class CommerceHintTimeoutTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"cart-extraction-timeout", "0"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Linux, ChromeOS and Windows: https://crbug.com/1257964.
// Falky on Mac: https://crbug.com/1312849.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_ExtractCart DISABLED_ExtractCart
#else
#define MAYBE_ExtractCart ExtractCart
#endif
IN_PROC_BROWSER_TEST_F(CommerceHintTimeoutTest, MAYBE_ExtractCart) {
  NavigateToURL("https://www.guitarcenter.com/cart.html");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 1, 1);
}

class CommerceHintMaxCountTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"cart-extraction-max-count", "1"},
             // Extend timeout to avoid flakiness.
             {"cart-extraction-timeout", "1m"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Linux: https://crbug.com/1257964.
// See definition of MAYBE_ExtractCart above.
IN_PROC_BROWSER_TEST_F(CommerceHintMaxCountTest, MAYBE_ExtractCart) {
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  // Wait for trying to fetch extraction script from browser process.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 30);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleWithProducts);
#endif
  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 1);

  // This would have triggered another extraction if not limited by max count
  // per navigation.
  SendXHR("/add-to-cart", "product: 123");

  // Navigation resets count, so can do another extraction.
  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 2);
}

// Override add-to-cart pattern.
class CommerceHintAddToCartPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"add-to-cart-pattern", "(special|text)"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintAddToCartPatternTest, AddToCartByURL) {
  NavigateToURL("https://www.guitarcenter.com/Special?product=1");
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);

  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  NavigateToURL("https://www.guitarcenter.com/add-to-cart?product=1");
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 1);

  NavigateToURL("https://www.guitarcenter.com/Text?product=1");
  WaitForUmaCount("Commerce.Carts.AddToCartByURL", 2);
}

IN_PROC_BROWSER_TEST_F(CommerceHintAddToCartPatternTest, AddToCartByForm) {
  NavigateToURL("https://www.guitarcenter.com/");

  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_Special");
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);

  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);

  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_Text");
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 2);
}

// Override per-domain add-to-cart pattern.
class CommerceHintSkippAddToCartTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"skip-add-to-cart-mapping", R"({"guitarcenter.com": ".*"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintSkippAddToCartTest, AddToCartByForm) {
  NavigateToURL("https://www.guitarcenter.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);

  // Test AddToCart that is supposed to be skipped based on resources is now
  // overwritten.
  const cart_db::ChromeCartContentProto qvc_cart =
      BuildProto("qvc.com", "https://www.qvc.com/checkout/cart.html");
  const ShoppingCarts result = {{"qvc.com", qvc_cart}};
#endif
  NavigateToURL("https://www.qvc.com/");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(result);
#endif
}

#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
// Override per-domain and generic cart pattern.
class CommerceHintCartPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"cart-pattern", "chicken|egg"},
             {"cart-pattern-mapping",
              R"({"guitarcenter.com": "(special|text)lol"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40238349): Deflake this test.
IN_PROC_BROWSER_TEST_F(CommerceHintCartPatternTest, DISABLED_VisitCart) {
  // The test is flaky with same-site back/forward cache, presumably because it
  // doesn't expect a RenderView change on same-site navigations.
  // TODO(crbug.com/40217176): Investigate and fix this.
  content::DisableBackForwardCacheForTesting(
      web_contents(),
      content::BackForwardCache::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  NavigateToURL("https://www.guitarcenter.com/SpecialLoL");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
  NavigateToURL("https://www.guitarcenter.com/chicken");
  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  NavigateToURL("https://www.guitarcenter.com/TextLoL");
  WaitForUmaCount("Commerce.Carts.VisitCart", 2);

  // Unspecified domains fall back to generic pattern.
  NavigateToURL("https://www.example.com/SpecialLoL");
  NavigateToURL("https://www.example.com/cart.html");
  NavigateToURL("https://www.example.com/TextLoL");
  WaitForUmaCount("Commerce.Carts.VisitCart", 2);

  NavigateToURL("https://www.example.com/Chicken");
  WaitForUmaCount("Commerce.Carts.VisitCart", 3);

  NavigateToURL("https://www.example.com/Egg");
  WaitForUmaCount("Commerce.Carts.VisitCart", 4);
}
#endif  // BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)

// Override per-domain and generic checkout pattern.
class CommerceHintCheckoutPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"checkout-pattern", "meow|purr"},
             {"checkout-pattern-mapping",
              R"({"guitarcenter.com": "special|text"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintCheckoutPatternTest, VisitCheckout) {
  NavigateToURL("https://www.guitarcenter.com/Special");
  // URLs are checked against checkout twice.
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 2);

  NavigateToURL("https://www.guitarcenter.com/checkout/");
  NavigateToURL("https://www.guitarcenter.com/meow/");
  NavigateToURL("https://www.guitarcenter.com/purr/");
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 2);

  NavigateToURL("https://www.guitarcenter.com/Text");
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 4);

  // Unspecified domains fall back to generic pattern.
  NavigateToURL("https://www.example.com/Special");
  NavigateToURL("https://www.example.com/checkout/");
  NavigateToURL("https://www.example.com/Text");
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 4);

  NavigateToURL("https://www.example.com/Meow");
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 6);

  NavigateToURL("https://www.example.com/Purr");
  WaitForUmaCount("Commerce.Carts.VisitCheckout", 8);
}

// Override per-domain and generic purchase button pattern.
class CommerceHintPurchaseButtonPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"purchase-button-pattern", "meow|purr"},
             {"purchase-button-pattern-mapping",
              R"({"guitarcenter.com": "woof|bark"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintPurchaseButtonPatternTest, PurchaseByForm) {
  std::string url;
  auto test_button = [&](const char* button_text) {
    NavigateToURL(url);
    const std::string& script = base::StringPrintf(
        R"(
      const btn = document.getElementById('submit');
      btn.innerText = "%s";
      btn.click();
    )",
        button_text);
    ASSERT_TRUE(ExecJs(web_contents(), script));
    content::TestNavigationObserver load_observer(web_contents());
    load_observer.WaitForNavigationFinished();
  };
  url = "https://www.guitarcenter.com/purchase.html";

  test_button("Woof");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 1);

  test_button("Meow");
  test_button("Pay now");
  test_button("Purr");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 1);

  test_button("Bark");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 2);

  // Unspecified domains fall back to generic pattern.
  url = "https://www.example.com/purchase.html";

  test_button("Meow");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 3);

  test_button("Woof");
  test_button("Pay now");
  test_button("Bark");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 3);

  test_button("Purr");
  WaitForUmaCount("Commerce.Carts.PurchaseByPOST", 4);
}

class CommerceHintPurchaseURLPatternTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
            ntp_features::kNtpChromeCartModule,
#else
            commerce::kCommerceHintAndroid,
#endif
            {{"purchase-url-pattern-mapping",
              R"({"guitarcenter.com": "special|text"})"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintPurchaseURLPatternTest, PurchaseByURL) {
  NavigateToURL("https://www.guitarcenter.com/Special");
  WaitForUmaCount("Commerce.Carts.PurchaseByURL", 1);

  NavigateToURL("https://www.guitarcenter.com/Text");
  WaitForUmaCount("Commerce.Carts.PurchaseByURL", 2);
}

class CommerceHintOptimizeRendererTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
             ntp_features::kNtpChromeCartModule,
#else
             commerce::kCommerceHintAndroid,
#endif
             {{"cart-extraction-gap-time", "0s"}}},
         {optimization_guide::features::kOptimizationHints, {{}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Times out on multiple platforms. https://crbug.com/1258553
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)
#define MAYBE_CartExtractionSkipped DISABLED_CartExtractionSkipped
#else
#define MAYBE_CartExtractionSkipped CartExtractionSkipped
#endif
IN_PROC_BROWSER_TEST_F(CommerceHintOptimizeRendererTest,
                       MAYBE_CartExtractionSkipped) {
  // Without adding testing hints, all the URLs are considered non-shopping.
  NavigateToURL("https://www.guitarcenter.com/cart.html");
#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  SendXHR("/add-to-cart", "product: 123");

  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 0);

  auto* optimization_guide_decider =
#if !BUILDFLAG(IS_ANDROID)
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
#else
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this));
#endif
  // Need the non-default port here.
  optimization_guide_decider->AddHintForTesting(
      https_server_.GetURL("www.guitarcenter.com", "/cart.html"),
      optimization_guide::proto::SHOPPING_PAGE_PREDICTOR, std::nullopt);

  NavigateToURL("https://www.guitarcenter.com/cart.html");
#if !BUILDFLAG(IS_ANDROID)
  WaitForCarts(kExpectedExample);
#endif
  SendXHR("/add-to-cart", "product: 123");

  WaitForUmaBucketCount("Commerce.Carts.ExtractionTimedOut", 0, 2);
}

#if !BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/40830409): This test is flaky on ChromeOS.
class CommerceHintAgentFencedFrameTest : public CommerceHintAgentTest {
 public:
  CommerceHintAgentFencedFrameTest() {
    scoped_feature_list_.InitWithFeatures(
        {
#if !BUILDFLAG(IS_ANDROID)
          ntp_features::kNtpChromeCartModule
#else
          commerce::kCommerceHintAndroid
#endif
        },
        {optimization_guide::features::kOptimizationHints});
  }

  void SetUpInProcessBrowserTestFixture() override {}

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintAgentFencedFrameTest,
                       VisitCartInFencedFrame) {
  // For add-to-cart by URL, normally a URL in that domain has already been
  // committed.
  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);

  // Create a fenced frame.
  GURL fenced_frame_url =
      https_server_.GetURL("www.guitarcenter.com", "/cart.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  // Do not affect counts.
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
class CommerceHintFeatureDefaultWithoutGeoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {}, {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintFeatureDefaultWithoutGeoTest,
                       DisableWithoutGeo) {
  ASSERT_FALSE(IsCartModuleEnabled());

  NavigateToURL("https://www.guitarcenter.com/cart.html");

  WaitForUmaCount("Commerce.Carts.VisitCart", 0);
  WaitForCartCount(kEmptyExpected);
}

class CommerceHintFeatureDefaultWithGeoTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {}, {optimization_guide::features::kOptimizationHints});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CommerceHintAgentTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "us");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintFeatureDefaultWithGeoTest, EnableWithGeo) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ASSERT_TRUE(IsCartModuleEnabled());

  NavigateToURL("https://www.guitarcenter.com/cart.html");
  WaitForUmaCount("Commerce.Carts.VisitCart", 1);
#else
  ASSERT_FALSE(IsCartModuleEnabled());

  WaitForUmaCount("Commerce.Carts.VisitCart", 0);
  WaitForCartCount(kEmptyExpected);
#endif
}
#endif

class CommerceHintDOMBasedHeuristicsTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
             ntp_features::kNtpChromeCartModule,
#else
             commerce::kCommerceHintAndroid,
#endif
             {}},
         {commerce::kChromeCartDomBasedHeuristics,
          {{"add-to-cart-button-active-time", "2s"},
           {"heuristics-execution-gap-time", "0s"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsTest,
                       DetectionRequiresAddToCartActive) {
  NavigateToURL("https://www.guitarcenter.com/product-page.html");
  // AddToCart requests without active AddToCart button focus.
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);

  // Focus on an AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 1);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 1);
}

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsTest,
                       DetectionInactiveForWrongButton) {
  NavigateToURL("https://www.guitarcenter.com/product-page.html");

  // Focus on a non-AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonTwo\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 1);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 1);

  // Focus on an AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 2);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 2);
}

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsTest,
                       AddToCartActiveExpires) {
  NavigateToURL("https://www.guitarcenter.com/product-page.html");

  // Focus on an AddToCart button, but wait until it's no longer active and then
  // send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  base::PlatformThread::Sleep(base::Seconds(2));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);

  // Focus on an AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonTwo\")"));
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 3);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 3);
}

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsTest,
                       TestAddToCartPattern) {
  NavigateToURL("https://www.guitarcenter.com/product-page.html");
  // Focus on an AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  SendXHR("/wp-admin/admin-ajax.php", "{\"sku\": \"123\", \"quantity\":1");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 1);
}

class CommerceHintDOMBasedHeuristicsSkipTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
             ntp_features::kNtpChromeCartModule,
#else
             commerce::kCommerceHintAndroid,
#endif
             {}},
         {commerce::kChromeCartDomBasedHeuristics,
          {{"skip-heuristics-domain-pattern", "guitarcenter"},
           {"add-to-cart-button-active-time", "0s"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsSkipTest,
                       SkipDOMBasedHeuristics) {
  // Send AddToCart requests on a skipped domain without active AddToCart
  // button.
  NavigateToURL("https://www.guitarcenter.com/product-page.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 1);

  // Send AddToCart requests on a skipped domain after focusing on a invalid
  // AddToCart button.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonTwo\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 2);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 0);

  // Send AddToCart requests on a non-skipped domain without active AddToCart
  // button.
  NavigateToURL("https://www.example.com/product-page.html");
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kExpectedExampleFallbackCart);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 2);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 0);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 0);
}

class CommerceHintDOMBasedHeuristicsGapTimeTest : public CommerceHintAgentTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
#if !BUILDFLAG(IS_ANDROID)
             ntp_features::kNtpChromeCartModule,
#else
             commerce::kCommerceHintAndroid,
#endif
             {}},
         {commerce::kChromeCartDomBasedHeuristics,
          {{"add-to-cart-button-active-time", "2s"},
           {"heuristics-execution-gap-time", "100s"}}}},
        {optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CommerceHintDOMBasedHeuristicsGapTimeTest,
                       EnforceExecutionTimeGap) {
  NavigateToURL("https://www.guitarcenter.com/product-page.html");

  // Focus on a non-AddToCart button and then send AddToCart requests.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonTwo\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 1);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 1);

  // Focus on an AddToCart button and then send AddToCart requests. Since the
  // gap time is shorter than the threshold, this focus event will be ignored.
  EXPECT_EQ(nullptr,
            content::EvalJs(web_contents(), "focusElement(\"buttonOne\")"));
  SendXHR("/wp-admin/admin-ajax.php", "action: woocommerce_add_to_cart");

#if !BUILDFLAG(IS_ANDROID)
  WaitForCartCount(kEmptyExpected);
#endif
  WaitForUmaCount("Commerce.Carts.AddToCartByPOST", 0);
  WaitForUmaCount("Commerce.Carts.AddToCartButtonDetection", 1);
  ExpectUKMCount(AddToCartEntry::kEntryName, "HeuristicsExecutionTime", 1);
}

}  // namespace
