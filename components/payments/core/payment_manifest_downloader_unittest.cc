// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/payments/core/error_logger.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using testing::_;

static constexpr char kNoContent[] = "";
static constexpr char kNoError[] = "";
static constexpr base::nullopt_t kNoLinkHeader = base::nullopt;
static constexpr char kEmptyLinkHeader[] = "";
static constexpr char kNoResponseBody[] = "";

}  // namespace

class PaymentMethodManifestDownloaderTest : public testing::Test {
 protected:
  enum class Headers {
    kSend,
    kOmit,
  };

  PaymentMethodManifestDownloaderTest()
      : test_url_("https://bobpay.com"),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)),
        downloader_(std::make_unique<ErrorLogger>(),
                    shared_url_loader_factory_) {
    downloader_.DownloadPaymentMethodManifest(
        url::Origin::Create(GURL("https://chromium.org")), test_url_,
        base::BindOnce(&PaymentMethodManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }

  MOCK_METHOD3(OnManifestDownload,
               void(const GURL& unused_url_after_redirects,
                    const std::string& content,
                    const std::string& error_message));

  void ServerResponse(int response_code,
                      Headers send_headers,
                      base::Optional<std::string> link_header,
                      const std::string& response_body,
                      int net_error) {
    scoped_refptr<net::HttpResponseHeaders> headers;
    if (send_headers == Headers::kSend) {
      headers = base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
      headers->ReplaceStatusLine(base::StringPrintf(
          "HTTP/1.1 %d %s", response_code,
          net::GetHttpReasonPhrase(
              static_cast<net::HttpStatusCode>(response_code))));

      if (link_header.has_value())
        headers->SetHeader("Link", *link_header);
    }

    downloader_.OnURLLoaderCompleteInternal(
        downloader_.GetLoaderForTesting(),
        downloader_.GetLoaderOriginalURLForTesting(), response_body, headers,
        net_error);
  }

  void ServerRedirect(int redirect_code, const GURL& new_url) {
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
};

TEST_F(PaymentMethodManifestDownloaderTest, FirstHttpResponse404IsFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.com/\"."));

  ServerResponse(404, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoHttpHeadersAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoHttpHeadersButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpHeaderAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(
                 _, kNoContent,
                 "No content and no \"Link: rel=payment-method-manifest\" HTTP "
                 "header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "response content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpLinkHeaderWithoutResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend, kEmptyLinkHeader, kNoResponseBody,
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerResponse(200, Headers::kSend, kEmptyLinkHeader, "response body",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoRelInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, std::string(),
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>", kNoResponseBody,
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoRelInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerResponse(200, Headers::kSend, "<manifest.json>", "response body",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoUrlInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend, "rel=payment-method-manifest",
                 kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoUrlInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerResponse(200, Headers::kSend, "rel=payment-method-manifest",
                 "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoManifestRellInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>; rel=web-app-manifest",
                 kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoManifestRellInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerResponse(200, Headers::kSend, "<manifest.json>; rel=web-app-manifest",
                 "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, SecondHttpResponse404IsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(404, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, EmptySecondResponseIsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "No content found in payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       SecondResponseWithoutHeadersIsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, NonEmptySecondResponseIsSuccess) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       InsufficientResourcesInHttpLinkFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.com/\"."));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       InsufficientResourcesAfterHttpLinkFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(PaymentMethodManifestDownloaderTest, FirstResponseCode204IsSuccess) {
  ServerResponse(204, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, SecondResponseCode204IsFailure) {
  ServerResponse(204, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "Unable to download payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(204, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       SecondResponseWithLinkHeaderAndNoContentIsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "No content found in payment manifest "
                                 "\"https://bobpay.com/manifest.json\"."));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       SecondResponseWithLinkHeaderAndWithContentReturnsTheContent) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 "manifest content", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, RelativeHttpHeaderLinkUrl) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_EQ("https://bobpay.com/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpsHeaderLinkUrl) {
  ServerResponse(200, Headers::kSend,
                 "<https://bobpay.com/manifest.json>; "
                 "rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_EQ("https://bobpay.com/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpHeaderLinkUrl) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "\"http://bobpay.com/manifest.json\" is not a valid payment manifest "
          "URL with HTTPS scheme (or HTTP scheme for localhost)."));

  ServerResponse(
      200, Headers::kSend,
      "<http://bobpay.com/manifest.json>; rel=payment-method-manifest",
      kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, 300IsUnsupportedRedirect) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "HTTP status code 300 \"Multiple Choices\" not allowed for "
                  "payment method manifest \"https://bobpay.com/\"."));

  ServerRedirect(300, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 301And302AreSupportedRedirects) {
  ServerRedirect(301, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  ServerRedirect(302, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       CannotRedirectAfterFollowingLinkHeader) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Unable to download the payment manifest because "
                         "reached the maximum number of redirects."));

  ServerRedirect(301, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 302And303AreSupportedRedirects) {
  ServerRedirect(302, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  ServerRedirect(303, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, 304IsUnsupportedRedirect) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "HTTP status code 304 \"Not Modified\" not allowed for "
                  "payment method manifest \"https://bobpay.com/\"."));

  ServerRedirect(304, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 305IsUnsupportedRedirect) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "HTTP status code 305 \"Use Proxy\" not allowed for "
                         "payment method manifest \"https://bobpay.com/\"."));

  ServerRedirect(305, GURL("https://pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 307And308AreSupportedRedirects) {
  ServerRedirect(307, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  ServerRedirect(308, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, NoMoreThanThreeRedirects) {
  ServerRedirect(301, GURL("https://pay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.com"));

  ServerRedirect(302, GURL("https://oldpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://oldpay.bobpay.com"));

  ServerRedirect(308, GURL("https://newpay.bobpay.com"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.com"));

  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Unable to download the payment manifest because "
                         "reached the maximum number of redirects."));

  ServerRedirect(308, GURL("https://newpay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, InvalidRedirectUrlIsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "\"\" is not a valid payment manifest URL with HTTPS "
                         "scheme (or HTTP scheme for localhost)."));

  ServerRedirect(308, GURL("pay.bobpay.com"));
}

TEST_F(PaymentMethodManifestDownloaderTest, NotAllowCrossSiteRedirects) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Cross-site redirect from \"https://bobpay.com/\" to "
          "\"https://alicepay.com/\" not allowed for payment manifests."));

  ServerRedirect(301, GURL("https://alicepay.com"));
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
        url::Origin::Create(test_url_), test_url_,
        base::BindOnce(&WebAppManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }

  ~WebAppManifestDownloaderTest() override {}

  MOCK_METHOD3(OnManifestDownload,
               void(const GURL& url,
                    const std::string& content,
                    const std::string& error_message));

  void ServerResponse(int response_code,
                      const std::string& response_body,
                      int net_error) {
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
    headers->ReplaceStatusLine(base::StringPrintf(
        "HTTP/1.1 %d %s", response_code,
        net::GetHttpReasonPhrase(
            static_cast<net::HttpStatusCode>(response_code))));
    downloader_.OnURLLoaderCompleteInternal(downloader_.GetLoaderForTesting(),
                                            test_url_, response_body, headers,
                                            net_error);
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
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.com/\"."));

  ServerResponse(404, kNoResponseBody, net::OK);
}

TEST_F(WebAppManifestDownloaderTest, EmptyHttpGetResponseIsFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "No content found in payment manifest \"https://bobpay.com/\"."));

  ServerResponse(200, kNoResponseBody, net::OK);
}

TEST_F(WebAppManifestDownloaderTest, NonEmptyHttpGetResponseIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, "manifest content", net::OK);
}

TEST_F(WebAppManifestDownloaderTest, InsufficientResourcesFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.com/\"."));

  ServerResponse(200, "manifest content", net::ERR_INSUFFICIENT_RESOURCES);
}

}  // namespace payments
