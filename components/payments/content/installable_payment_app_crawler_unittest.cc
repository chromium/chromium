// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/installable_payment_app_crawler.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/payment_manifest_downloader.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/const_csp_checker.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::NiceMock;

constexpr char kPaymentMethodManifestUrl[] =
    "https://bobpay.test/payment-manifest.json";
constexpr char kWebAppManifestUrl[] = "https://bobpay.test/app.json";
constexpr char kPaymentMethodManifestLinkHeader[] =
    "<https://bobpay.test/payment-manifest.json>; "
    "rel=\"payment-method-manifest\"";
constexpr char kPaymentMethodManifestResponseBody[] = R"({
  "default_applications": ["https://bobpay.test/app.json"]
})";

std::string CreateWebAppManifestResponseBody(std::string_view sw_src,
                                             std::string_view sw_scope) {
  return base::StringPrintf(R"({
    "name": "Bob Pay",
    "icons": [{
      "src": "icon.png",
      "sizes": "48x48",
      "type": "image/png"
    }],
    "serviceworker": {
      "src": "%s",
      "scope": "%s"
    }
  })",
                            std::string(sw_src).c_str(),
                            std::string(sw_scope).c_str());
}

class InstallablePaymentAppCrawlerTest
    : public content::RenderViewHostTestHarness {
 public:
  InstallablePaymentAppCrawlerTest()
      : test_manifest_url_("https://bobpay.test/method.json"),
        merchant_origin_(url::Origin::Create(GURL("https://merchant.test"))),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)),
        const_csp_checker_(std::make_unique<ConstCSPChecker>(/*allow=*/true)) {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Grant PAYMENT_HANDLER permission by default so the crawler can proceed
    // with downloading and validating web app manifests.
    auto mock_permission_manager =
        std::make_unique<NiceMock<content::MockPermissionManager>>();
    ON_CALL(*mock_permission_manager,
            GetPermissionResultForOriginWithoutContext(_, _, _))
        .WillByDefault(testing::Return(content::PermissionResult(
            blink::mojom::PermissionStatus::GRANTED,
            content::PermissionStatusSource::UNSPECIFIED)));
    static_cast<content::TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(std::move(mock_permission_manager));

    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_rfh;
    test_factory_.Clone(url_loader_factory_rfh.BindNewPipeAndPassReceiver());

    downloader_ = std::make_unique<PaymentManifestDownloader>(
        std::make_unique<DeveloperConsoleLogger>(web_contents()),
        const_csp_checker_->GetWeakPtr(), shared_url_loader_factory_,
        std::move(url_loader_factory_rfh), main_rfh()->GetWeakDocumentPtr());

    parser_ = std::make_unique<PaymentManifestParser>(
        std::make_unique<DeveloperConsoleLogger>(web_contents()));

    crawler_ = std::make_unique<InstallablePaymentAppCrawler>(
        merchant_origin_, main_rfh(), downloader_.get(), parser_.get(),
        /*cache=*/nullptr);
  }

  const std::vector<std::string>& logged_errors() {
    return content::RenderFrameHostTester::For(main_rfh())
        ->GetConsoleMessages();
  }

  void SetPaymentMethodManifestResponse(
      int response_code,
      const std::optional<std::string>& link_header,
      const std::string& response_body,
      int net_error = net::OK) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    head->headers->ReplaceStatusLine(
        base::StringPrintf("HTTP/1.1 %d %s", response_code,
                           net::GetHttpReasonPhrase(response_code)));
    if (link_header.has_value()) {
      head->headers->SetHeader("Link", *link_header);
    }
    test_factory_.AddResponse(test_manifest_url_, std::move(head),
                              response_body,
                              network::URLLoaderCompletionStatus(net_error));
  }

  void SetResponseBodyResponse(const GURL& url,
                               int response_code,
                               const std::string& response_body,
                               int net_error = net::OK) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    head->headers->ReplaceStatusLine(
        base::StringPrintf("HTTP/1.1 %d %s", response_code,
                           net::GetHttpReasonPhrase(response_code)));
    test_factory_.AddResponse(url, std::move(head), response_body,
                              network::URLLoaderCompletionStatus(net_error));
  }

  void StartCrawler(bool complete_icon_download = false) {
    base::RunLoop run_loop;
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    method_data.push_back(mojom::PaymentMethodData::New());
    method_data.back()->supported_method = test_manifest_url_.spec();
    crawler_->Start(
        method_data, /*method_manifest_urls_for_metadata_refresh=*/{},
        base::BindOnce(
            &InstallablePaymentAppCrawlerTest::OnPaymentAppCrawlingFinished,
            base::Unretained(this))
            .Then(run_loop.QuitClosure()),
        base::DoNothing());
    if (complete_icon_download) {
      GURL icon_url("https://bobpay.test/icon.png");
      ASSERT_TRUE(base::test::RunUntil([&] {
        return content::WebContentsTester::For(web_contents())
            ->HasPendingDownloadImage(icon_url);
      }));
      SkBitmap bitmap;
      bitmap.allocN32Pixels(32, 32);
      content::WebContentsTester::For(web_contents())
          ->TestDidDownloadImage(icon_url, /*http_status_code=*/200, {bitmap},
                                 {gfx::Size(32, 32)});
    }
    run_loop.Run();
  }

  MOCK_METHOD(void,
              OnPaymentAppCrawlingFinished,
              ((std::map<GURL, std::unique_ptr<WebAppInstallationInfo>>),
               (std::map<GURL, std::unique_ptr<RefetchedMetadata>>),
               const std::string&));

 protected:
  GURL test_manifest_url_;
  url::Origin merchant_origin_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<ConstCSPChecker> const_csp_checker_;
  std::unique_ptr<PaymentManifestDownloader> downloader_;
  std::unique_ptr<PaymentManifestParser> parser_;
  std::unique_ptr<InstallablePaymentAppCrawler> crawler_;
};

TEST_F(InstallablePaymentAppCrawlerTest,
       ManifestDownloaderFailureReturnsGenericError) {
  SetPaymentMethodManifestResponse(404, /*link_header=*/std::nullopt,
                                   /*response_body=*/"", net::OK);

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Unable to download payment manifest "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("Unable to download payment manifest "
                  "\"https://bobpay.test/method.json\". HTTP 404 Not Found."));
}

TEST_F(InstallablePaymentAppCrawlerTest,
       WebAppManifestDownloadFailureReturnsGenericError) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(GURL(kWebAppManifestUrl), 404, "");

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("Unable to download payment manifest "
                  "\"https://bobpay.test/app.json\". HTTP 404 Not Found."));
}

TEST_F(InstallablePaymentAppCrawlerTest, CrossOriginWebAppManifestFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          R"({
        "default_applications": ["https://alicepay.test/app.json"]
      })");

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre(
          "Cross-origin default application https://alicepay.test/app.json "
          "not allowed in payment method manifest "
          "https://bobpay.test/payment-manifest.json."));
}

TEST_F(InstallablePaymentAppCrawlerTest, CrossOriginServiceWorkerUrlFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(
      GURL(kWebAppManifestUrl), 200,
      CreateWebAppManifestResponseBody("https://alicepay.test/sw.js",
                                       "https://bobpay.test/scope"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre(
          "Cross-origin \"serviceworker\".\"src\" https://alicepay.test/sw.js "
          "not allowed in web app manifest https://bobpay.test/app.json."));
}

TEST_F(InstallablePaymentAppCrawlerTest, CrossOriginServiceWorkerScopeFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(
      GURL(kWebAppManifestUrl), 200,
      CreateWebAppManifestResponseBody("https://bobpay.test/sw.js",
                                       "https://alicepay.test/scope"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre(
          "Cross-origin \"serviceworker\".\"scope\" "
          "https://alicepay.test/scope "
          "not allowed in web app manifest https://bobpay.test/app.json."));
}

TEST_F(InstallablePaymentAppCrawlerTest, CannotResolveServiceWorkerUrlFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(GURL(kWebAppManifestUrl), 200,
                          CreateWebAppManifestResponseBody(
                              "https://", "https://bobpay.test/scope"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("Cannot resolve the \"serviceworker\".\"src\" https:// "
                  "in web app manifest https://bobpay.test/app.json."));
}

TEST_F(InstallablePaymentAppCrawlerTest, CannotResolveServiceWorkerScopeFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(GURL(kWebAppManifestUrl), 200,
                          CreateWebAppManifestResponseBody(
                              "https://bobpay.test/sw.js", "https://"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("Cannot resolve the \"serviceworker\".\"scope\" https:// "
                  "in web app manifest https://bobpay.test/app.json."));
}

TEST_F(InstallablePaymentAppCrawlerTest,
       ServiceWorkerScopeNotUnderMaxScopeFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(
      GURL(kWebAppManifestUrl), 200,
      CreateWebAppManifestResponseBody("https://bobpay.test/other/sw.js",
                                       "https://bobpay.test/scope/"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("The path of the provided scope ('/scope/') is not under the "
                  "max scope allowed ('/other/'). Adjust the scope or move the "
                  "Service Worker script."));
}

TEST_F(InstallablePaymentAppCrawlerTest, InvalidWebAppIconFails) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(GURL(kWebAppManifestUrl), 200,
                          R"({
        "name": "Bob Pay",
        "icons": [],
        "serviceworker": {
          "src": "https://bobpay.test/sw.js",
          "scope": "https://bobpay.test/scope"
        }
      })");

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished(
                         _, _,
                         "Failed to install the payment handler for "
                         "\"https://bobpay.test/method.json\"."));

  StartCrawler();

  EXPECT_THAT(
      logged_errors(),
      ElementsAre("No valid icon information for installable payment handler "
                  "found in web app manifest \"https://bobpay.test/app.json\" "
                  "for payment handler manifest "
                  "\"https://bobpay.test/method.json\".",
                  "Failed to download or decode a non-empty icon for payment "
                  "app with \"https://bobpay.test/app.json\" manifest."));
}

TEST_F(InstallablePaymentAppCrawlerTest,
       SuccessfulCrawlingReturnsInstallableApp) {
  SetPaymentMethodManifestResponse(200, kPaymentMethodManifestLinkHeader, "");
  SetResponseBodyResponse(GURL(kPaymentMethodManifestUrl), 200,
                          kPaymentMethodManifestResponseBody);
  SetResponseBodyResponse(
      GURL(kWebAppManifestUrl), 200,
      CreateWebAppManifestResponseBody("https://bobpay.test/sw.js",
                                       "https://bobpay.test/scope"));

  EXPECT_CALL(*this, OnPaymentAppCrawlingFinished)
      .WillOnce(
          [&](std::map<GURL, std::unique_ptr<WebAppInstallationInfo>> apps,
              std::map<GURL, std::unique_ptr<RefetchedMetadata>> metadata,
              const std::string& error_message) {
            EXPECT_TRUE(error_message.empty());
            EXPECT_TRUE(metadata.empty());
            ASSERT_EQ(apps.size(), 1u);
            auto it = apps.find(test_manifest_url_);
            ASSERT_NE(it, apps.end());
            ASSERT_NE(it->second, nullptr);
            EXPECT_EQ(it->second->name, "Bob Pay");
            EXPECT_EQ(it->second->sw_js_url, "https://bobpay.test/sw.js");
            EXPECT_EQ(it->second->sw_scope, "https://bobpay.test/scope");
            ASSERT_NE(it->second->icon, nullptr);
            EXPECT_FALSE(it->second->icon->drawsNothing());
            EXPECT_EQ(it->second->icon->width(), 32);
            EXPECT_EQ(it->second->icon->height(), 32);
          });

  StartCrawler(/*complete_icon_download=*/true);

  EXPECT_TRUE(logged_errors().empty());
}

}  // namespace
}  // namespace payments
