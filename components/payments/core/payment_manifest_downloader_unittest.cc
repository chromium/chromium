// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/payments/core/error_logger.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using testing::_;

static const char kNoError[] = "";

}  // namespace

class PaymentMethodManifestDownloaderTest : public testing::Test {
 public:
  PaymentMethodManifestDownloaderTest()
      : test_url_("https://bobpay.com"),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)),
        downloader_(std::make_unique<ErrorLogger>(),
                    shared_url_loader_factory_) {
    downloader_.DownloadPaymentMethodManifest(
        test_url_,
        base::BindOnce(&PaymentMethodManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }

  ~PaymentMethodManifestDownloaderTest() override {}

  MOCK_METHOD3(OnManifestDownload,
               void(const GURL& unused_url_after_redirects,
                    const std::string& content,
                    const std::string& error_message));

  void CallComplete(int response_code = 200,
                    const std::string& link_header = std::string(),
                    const std::string& response_body = std::string(),
                    bool send_headers = true) {
    scoped_refptr<net::HttpResponseHeaders> headers;
    if (send_headers) {
      headers = base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
      headers->ReplaceStatusLine(base::StringPrintf(
          "HTTP/1.1 %d %s", response_code,
          net::GetHttpReasonPhrase(
              static_cast<net::HttpStatusCode>(response_code))));
    }

    if (!link_header.empty())
      headers->AddHeader(link_header);
    downloader_.OnURLLoaderCompleteInternal(
        downloader_.GetLoaderForTesting(),
        downloader_.GetLoaderOriginalURLForTesting(), response_body, headers,
        net::OK);
  }

  void CallRedirect(int redirect_code, const GURL& new_url) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = redirect_code;
    redirect_info.new_url = new_url;
    std::vector<std::string> to_be_removed_headers;

    downloader_.OnURLLoaderRedirect(
        downloader_.GetLoaderForTesting(), redirect_info,
        network::mojom::URLResponseHead(), &to_be_removed_headers);
  }

  GURL GetOriginalURL() { return downloader_.GetLoaderOriginalURLForTesting(); }

 private:
  GURL test_url_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  PaymentManifestDownloader downloader_;

  DISALLOW_COPY_AND_ASSIGN(PaymentMethodManifestDownloaderTest);
};

TEST_F(PaymentMethodManifestDownloaderTest, HttpHeadResponse404IsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, std::string(),
                  "Unable to make a HEAD request to \"https://bobpay.com/\" "
                  "for payment method manifest."));

  CallComplete(404);
}

TEST_F(PaymentMethodManifestDownloaderTest, NoHttpHeadersIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200, std::string(), std::string(), false);
}

TEST_F(PaymentMethodManifestDownloaderTest, EmptyHttpHeaderIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200);
}

TEST_F(PaymentMethodManifestDownloaderTest, EmptyHttpLinkHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200, "Link:");
}

TEST_F(PaymentMethodManifestDownloaderTest, NoRelInHttpLinkHeaderIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200, "Link: <manifest.json>");
}

TEST_F(PaymentMethodManifestDownloaderTest, NoUrlInHttpLinkHeaderIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200, "Link: rel=payment-method-manifest");
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoManifestRellInHttpLinkHeaderIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(_, std::string(),
                                "No \"Link: rel=payment-method-manifest\" HTTP "
                                "header found at \"https://bobpay.com/\"."));

  CallComplete(200, "Link: <manifest.json>; rel=web-app-manifest");
}

TEST_F(PaymentMethodManifestDownloaderTest, HttpGetResponse404IsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this,
              OnManifestDownload(_, std::string(),
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  CallComplete(404);
}

TEST_F(PaymentMethodManifestDownloaderTest, EmptyHttpGetResponseIsFailure) {
  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this,
              OnManifestDownload(_, std::string(),
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  CallComplete(200, std::string(), std::string(), false);
}

TEST_F(PaymentMethodManifestDownloaderTest, NonEmptyHttpGetResponseIsSuccess) {
  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, std::string(), "manifest content");
}

TEST_F(PaymentMethodManifestDownloaderTest, HeaderResponseCode204IsSuccess) {
  CallComplete(204, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, std::string(), "manifest content");
}

TEST_F(PaymentMethodManifestDownloaderTest, RelativeHttpHeaderLinkUrl) {
  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_EQ("https://bobpay.com/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpsHeaderLinkUrl) {
  CallComplete(200,
               "Link: <https://bobpay.com/manifest.json>; "
               "rel=payment-method-manifest");

  EXPECT_EQ("https://bobpay.com/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpHeaderLinkUrl) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, std::string(),
          "\"http://bobpay.com/manifest.json\" is not a valid payment manifest "
          "URL with HTTPS scheme (or HTTP scheme for localhost)."));

  CallComplete(200,
               "Link: <http://bobpay.com/manifest.json>; "
               "rel=payment-method-manifest");
}

TEST_F(PaymentMethodManifestDownloaderTest, 300IsUnsupportedRedirect) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, std::string(),
                  "HTTP status code 300 \"Multiple Choices\" not allowed for "
                  "payment method manifest \"https://bobpay.com/\"."));

  CallRedirect(300, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 301And302AreSupportedRedirects) {
  CallRedirect(301, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  CallRedirect(302, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, std::string(), "manifest content");
}

TEST_F(PaymentMethodManifestDownloaderTest, 302And303AreSupportedRedirects) {
  CallRedirect(302, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  CallRedirect(303, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, std::string(), "manifest content");
}

TEST_F(PaymentMethodManifestDownloaderTest, 304IsUnsupportedRedirect) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, std::string(),
                  "HTTP status code 304 \"Not Modified\" not allowed for "
                  "payment method manifest \"https://bobpay.com/\"."));

  CallRedirect(304, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 305IsUnsupportedRedirect) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, std::string(),
                         "HTTP status code 305 \"Use Proxy\" not allowed for "
                         "payment method manifest \"https://bobpay.com/\"."));

  CallRedirect(305, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 307And308AreSupportedRedirects) {
  CallRedirect(307, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  CallRedirect(308, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  CallComplete(200, "Link: <manifest.json>; rel=payment-method-manifest");

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, std::string(), "manifest content");
}

TEST_F(PaymentMethodManifestDownloaderTest, NoMoreThanThreeRedirects) {
  CallRedirect(301, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  CallRedirect(302, GURL("https://oldpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://oldpay.bobpay.com"));

  CallRedirect(308, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  EXPECT_CALL(*this, OnManifestDownload(
                         _, std::string(),
                         "Unable to download the payment manifest because "
                         "reached the maximum number of redirects."));

  CallRedirect(308, GURL("https://newpay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, InvalidRedirectUrlIsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, std::string(),
                         "\"\" is not a valid payment manifest URL with HTTPS "
                         "scheme (or HTTP scheme for localhost)."));

  CallRedirect(308, GURL("pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, NotAllowCrossSiteRedirects) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, std::string(),
          "Cross-site redirect from \"https://bobpay.com/\" to "
          "\"https://alicepay.com/\" not allowed for payment manifests."));

  CallRedirect(301, GURL("https://alicepay.com"));
}

class WebAppManifestDownloaderTest : public testing::Test {
 public:
  WebAppManifestDownloaderTest()
      : test_url_("https://bobpay.com"),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)),
        downloader_(std::make_unique<ErrorLogger>(),
                    shared_url_loader_factory_) {
    downloader_.DownloadWebAppManifest(
        test_url_,
        base::BindOnce(&WebAppManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }

  ~WebAppManifestDownloaderTest() override {}

  MOCK_METHOD3(OnManifestDownload,
               void(const GURL& url,
                    const std::string& content,
                    const std::string& error_message));

  void CallComplete(int response_code,
                    const std::string& response_body = std::string()) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
    headers->ReplaceStatusLine(base::StringPrintf(
        "HTTP/1.1 %d %s", response_code,
        net::GetHttpReasonPhrase(
            static_cast<net::HttpStatusCode>(response_code))));
    downloader_.OnURLLoaderCompleteInternal(downloader_.GetLoaderForTesting(),
                                            test_url_, response_body, headers,
                                            net::OK);
  }

 private:
  GURL test_url_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  PaymentManifestDownloader downloader_;

  DISALLOW_COPY_AND_ASSIGN(WebAppManifestDownloaderTest);
};

TEST_F(WebAppManifestDownloaderTest, HttpGetResponse404IsFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, std::string(),
          "Unable to download payment manifest \"https://bobpay.com/\"."));

  CallComplete(404);
}

TEST_F(WebAppManifestDownloaderTest, EmptyHttpGetResponseIsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(_, std::string(), kNoError));

  CallComplete(200);
}

TEST_F(WebAppManifestDownloaderTest, NonEmptyHttpGetResponseIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  CallComplete(200, "manifest content");
}

}  // namespace payments
