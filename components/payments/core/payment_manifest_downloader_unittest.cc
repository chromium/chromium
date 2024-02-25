// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/payments/core/const_csp_checker.h"
#include "components/payments/core/error_logger.h"
#include "components/payments/core/features.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using testing::_;

static constexpr char kNoContent[] = "";
static constexpr char kNoError[] = "";
static constexpr std::nullopt_t kNoLinkHeader = std::nullopt;
static constexpr char kEmptyLinkHeader[] = "";
static constexpr char kNoResponseBody[] = "";

}  // namespace

class PaymentManifestDownloaderTestBase : public testing::Test {
 public:
  enum class Headers {
    kSend,
    kOmit,
  };

  PaymentManifestDownloaderTestBase()
      : test_url_("https://bobpay.test"),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)),
        const_csp_checker_(std::make_unique<ConstCSPChecker>(/*allow=*/true)) {}

  void InitDownloader() {
    downloader_ = std::make_unique<PaymentManifestDownloader>(
        std::make_unique<ErrorLogger>(), const_csp_checker_->GetWeakPtr(),
        shared_url_loader_factory_);
  }

  MOCK_METHOD3(OnManifestDownload,
               void(const GURL& unused_url_after_redirects,
                    const std::string& content,
                    const std::string& error_message));

  void ServerResponse(int response_code,
                      Headers send_headers,
                      std::optional<std::string> link_header,
                      const std::string& response_body,
                      int net_error) {
    scoped_refptr<net::HttpResponseHeaders> headers;
    if (send_headers == Headers::kSend) {
      headers = base::MakeRefCounted<net::HttpResponseHeaders>(std::string());
      headers->ReplaceStatusLine(base::StringPrintf(
          "HTTP/1.1 %d %s", response_code,
          net::GetHttpReasonPhrase(
              static_cast<net::HttpStatusCode>(response_code))));

      if (link_header.has_value()) {
        headers->SetHeader("Link", *link_header);
      }
    }

    downloader_->OnURLLoaderCompleteInternal(
        downloader_->GetLoaderForTesting(),
        downloader_->GetLoaderOriginalURLForTesting(), response_body, headers,
        net_error);
  }

  void ServerResponse(int response_code,
                      const std::string& response_body,
                      int net_error) {
    ServerResponse(response_code, Headers::kSend, /*link_header=*/std::nullopt,
                   response_body, net_error);
  }

  void ServerRedirect(int redirect_code, const GURL& new_url) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = redirect_code;
    redirect_info.new_url = new_url;
    std::vector<std::string> to_be_removed_headers;
    // This is irrelevant.
    GURL url_before_redirect;

    downloader_->OnURLLoaderRedirect(
        downloader_->GetLoaderForTesting(), url_before_redirect, redirect_info,
        network::mojom::URLResponseHead(), &to_be_removed_headers);
  }

  GURL GetOriginalURL() {
    return downloader_->GetLoaderOriginalURLForTesting();
  }

 protected:
  GURL test_url_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<ConstCSPChecker> const_csp_checker_;
  std::unique_ptr<PaymentManifestDownloader> downloader_;
};

class PaymentMethodManifestDownloaderTest
    : public PaymentManifestDownloaderTestBase {
 public:
  PaymentMethodManifestDownloaderTest() {
    InitDownloader();
    downloader_->DownloadPaymentMethodManifest(
        url::Origin::Create(GURL("https://chromium.org")), test_url_,
        base::BindOnce(&PaymentMethodManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }
};

TEST_F(PaymentMethodManifestDownloaderTest, FirstHttpResponse404IsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Unable to download payment manifest "
                         "\"https://bobpay.test/\". HTTP 404 Not Found."));

  ServerResponse(404, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoHttpHeadersAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoHttpHeadersButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpHeaderAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpHeaderButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "response content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpLinkHeaderWithoutResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, kEmptyLinkHeader, kNoResponseBody,
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       EmptyHttpLinkHeaderButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, kEmptyLinkHeader, "response body",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoRelInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>", kNoResponseBody,
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoRelInHttpLinkHeaderButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>", "response body",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoUrlInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "rel=payment-method-manifest",
                 kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoUrlInHttpLinkHeaderButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "rel=payment-method-manifest",
                 "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoManifestRellInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>; rel=web-app-manifest",
                 kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       NoManifestRellInHttpLinkHeaderButWithResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No \"Link: rel=payment-method-manifest\" HTTP header found "
                  "at \"https://bobpay.test/\"."));

  ServerResponse(200, Headers::kSend, "<manifest.json>; rel=web-app-manifest",
                 "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, SecondHttpResponse404IsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(
      *this, OnManifestDownload(
                 _, kNoContent,
                 "Unable to download payment manifest "
                 "\"https://bobpay.test/manifest.json\". HTTP 404 Not Found."));

  ServerResponse(404, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, EmptySecondResponseIsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "No content found in payment manifest "
                                 "\"https://bobpay.test/manifest.json\"."));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       SecondResponseWithoutHeadersButWithContentIsSuccess) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "response content", kNoError));

  ServerResponse(200, Headers::kOmit, kNoLinkHeader, "response content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest,
       SecondResponseWithoutContentIsFailure) {
  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this,
              OnManifestDownload(_, kNoContent,
                                 "No content found in payment manifest "
                                 "\"https://bobpay.test/manifest.json\"."));

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
      *this, OnManifestDownload(
                 _, kNoContent,
                 "Unable to download payment manifest "
                 "\"https://bobpay.test/\". ERR_INSUFFICIENT_RESOURCES (-12)"));

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
                                 "\"https://bobpay.test/manifest.json\". "
                                 "ERR_INSUFFICIENT_RESOURCES (-12)"));

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

  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest "
          "\"https://bobpay.test/manifest.json\". HTTP 204 No Content."));

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
                                 "\"https://bobpay.test/manifest.json\"."));

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

  EXPECT_EQ("https://bobpay.test/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpsHeaderLinkUrl) {
  ServerResponse(200, Headers::kSend,
                 "<https://bobpay.test/manifest.json>; "
                 "rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_EQ("https://bobpay.test/manifest.json", GetOriginalURL());
}

TEST_F(PaymentMethodManifestDownloaderTest, AbsoluteHttpHeaderLinkUrl) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "\"http://bobpay.test/manifest.json\" is not a valid payment "
                  "manifest "
                  "URL with HTTPS scheme (or HTTP scheme for localhost)."));

  ServerResponse(
      200, Headers::kSend,
      "<http://bobpay.test/manifest.json>; rel=payment-method-manifest",
      kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, 300IsUnsupportedRedirect) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "HTTP status code 300 \"Multiple Choices\" not allowed for "
                  "payment method manifest \"https://bobpay.test/\"."));

  ServerRedirect(300, GURL("https://pay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 301And302AreSupportedRedirects) {
  ServerRedirect(301, GURL("https://pay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.test"));

  ServerRedirect(302, GURL("https://newpay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.test"));

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

  ServerRedirect(301, GURL("https://pay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 302And303AreSupportedRedirects) {
  ServerRedirect(302, GURL("https://pay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.test"));

  ServerRedirect(303, GURL("https://newpay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.test"));

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
                  "payment method manifest \"https://bobpay.test/\"."));

  ServerRedirect(304, GURL("https://pay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 305IsUnsupportedRedirect) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "HTTP status code 305 \"Use Proxy\" not allowed for "
                         "payment method manifest \"https://bobpay.test/\"."));

  ServerRedirect(305, GURL("https://pay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, 307And308AreSupportedRedirects) {
  ServerRedirect(307, GURL("https://pay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.test"));

  ServerRedirect(308, GURL("https://newpay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.test"));

  ServerResponse(200, Headers::kSend,
                 "<manifest.json>; rel=payment-method-manifest",
                 kNoResponseBody, net::OK);

  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, Headers::kSend, kNoLinkHeader, "manifest content",
                 net::OK);
}

TEST_F(PaymentMethodManifestDownloaderTest, NoMoreThanThreeRedirects) {
  ServerRedirect(301, GURL("https://pay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://pay.bobpay.test"));

  ServerRedirect(302, GURL("https://oldpay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://oldpay.bobpay.test"));

  ServerRedirect(308, GURL("https://newpay.bobpay.test"));

  EXPECT_EQ(GetOriginalURL(), GURL("https://newpay.bobpay.test"));

  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Unable to download the payment manifest because "
                         "reached the maximum number of redirects."));

  ServerRedirect(308, GURL("https://newpay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, InvalidRedirectUrlIsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "\"\" is not a valid payment manifest URL with HTTPS "
                         "scheme (or HTTP scheme for localhost)."));

  ServerRedirect(308, GURL("pay.bobpay.test"));
}

TEST_F(PaymentMethodManifestDownloaderTest, NotAllowCrossSiteRedirects) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Cross-site redirect from \"https://bobpay.test/\" to "
          "\"https://alicepay.test/\" not allowed for payment manifests."));

  ServerRedirect(301, GURL("https://alicepay.test"));
}

// Variant of PaymentMethodManifestDownloaderTest covering the logic when
// kPaymentHandlerRequireLinkHeader is set to false.
class PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest
    : public PaymentManifestDownloaderTestBase {
 public:
  PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kPaymentHandlerRequireLinkHeader);
    InitDownloader();
    downloader_->DownloadPaymentMethodManifest(
        url::Origin::Create(GURL("https://chromium.org")), test_url_,
        base::BindOnce(&PaymentManifestDownloaderTestBase::OnManifestDownload,
                       base::Unretained(this)));
  }

  // Simulates two responses for payment method manifest download:
  // 1) Only HTTP header without the response body content, responding to the
  //    initial HEAD request.
  // 2) Both HTTP header and the response body content, for the subsequent GET
  //    request.
  void ServerHeaderAndFallbackResponse(int response_code,
                                       Headers send_headers,
                                       std::optional<std::string> link_header,
                                       const std::string& response_body,
                                       int net_error) {
    ServerResponse(response_code, send_headers, link_header, kNoResponseBody,
                   net_error);
    ServerResponse(response_code, send_headers, link_header, response_body,
                   net_error);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoHttpHeadersAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kOmit, kNoLinkHeader,
                                  kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoHttpHeadersButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kOmit, kNoLinkHeader,
                                  "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       EmptyHttpHeaderAndEmptyResponseBodyIsFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(
                 _, kNoContent,
                 "No content and no \"Link: rel=payment-method-manifest\" HTTP "
                 "header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, kNoLinkHeader,
                                  kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       EmptyHttpHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response content", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, kNoLinkHeader,
                                  "response content", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       EmptyHttpLinkHeaderWithoutResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, kEmptyLinkHeader,
                                  kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       EmptyHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, kEmptyLinkHeader,
                                  "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoRelInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, std::string(),
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, "<manifest.json>",
                                  kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoUrlInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kSend,
                                  "rel=payment-method-manifest",
                                  "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoManifestRellInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kSend,
                                  "<manifest.json>; rel=web-app-manifest",
                                  kNoResponseBody, net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoManifestRellInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kSend,
                                  "<manifest.json>; rel=web-app-manifest",
                                  "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoRelInHttpLinkHeaderButWithResponseBodyIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "response body", kNoError));

  ServerHeaderAndFallbackResponse(200, Headers::kSend, "<manifest.json>",
                                  "response body", net::OK);
}

TEST_F(PaymentMethodManifestDownloaderLinkHeaderNotRequiredTest,
       NoUrlInHttpLinkHeaderAndNoResponseBodyIsFailure) {
  EXPECT_CALL(*this,
              OnManifestDownload(
                  _, kNoContent,
                  "No content and no \"Link: rel=payment-method-manifest\" "
                  "HTTP header found at \"https://bobpay.test/\"."));

  ServerHeaderAndFallbackResponse(200, Headers::kSend,
                                  "rel=payment-method-manifest",
                                  kNoResponseBody, net::OK);
}

class WebAppManifestDownloaderTest : public PaymentManifestDownloaderTestBase {
 public:
  WebAppManifestDownloaderTest() {
    InitDownloader();
    downloader_->DownloadWebAppManifest(
        url::Origin::Create(test_url_), test_url_,
        base::BindOnce(&WebAppManifestDownloaderTest::OnManifestDownload,
                       base::Unretained(this)));
  }
};

TEST_F(WebAppManifestDownloaderTest, HttpGetResponse404IsFailure) {
  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Unable to download payment manifest "
                         "\"https://bobpay.test/\". HTTP 404 Not Found."));

  ServerResponse(404, kNoResponseBody, net::OK);
}

TEST_F(WebAppManifestDownloaderTest, EmptyHttpGetResponseIsFailure) {
  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "No content found in payment manifest \"https://bobpay.test/\"."));

  ServerResponse(200, kNoResponseBody, net::OK);
}

TEST_F(WebAppManifestDownloaderTest, NonEmptyHttpGetResponseIsSuccess) {
  EXPECT_CALL(*this, OnManifestDownload(_, "manifest content", kNoError));

  ServerResponse(200, "manifest content", net::OK);
}

TEST_F(WebAppManifestDownloaderTest, InsufficientResourcesFailure) {
  EXPECT_CALL(
      *this, OnManifestDownload(
                 _, kNoContent,
                 "Unable to download payment manifest "
                 "\"https://bobpay.test/\". ERR_INSUFFICIENT_RESOURCES (-12)"));

  ServerResponse(200, "manifest content", net::ERR_INSUFFICIENT_RESOURCES);
}

using PaymentManifestDownloaderCSPTest = PaymentManifestDownloaderTestBase;

// Download fails when CSP checker is gone, e.g., during shutdown.
TEST_F(PaymentManifestDownloaderCSPTest,
       PaymentMethodManifestCSPCheckerMissing) {
  InitDownloader();
  const_csp_checker_.reset();

  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.test/\"."));

  downloader_->DownloadPaymentMethodManifest(
      url::Origin::Create(GURL("https://chromium.org")), test_url_,
      base::BindOnce(&PaymentManifestDownloaderCSPTest::OnManifestDownload,
                     base::Unretained(this)));
}

// Download fails when CSP checker is gone, e.g., during shutdown.
TEST_F(PaymentManifestDownloaderCSPTest, WebAppManifestCSPCheckerMissing) {
  InitDownloader();
  const_csp_checker_.reset();

  EXPECT_CALL(
      *this,
      OnManifestDownload(
          _, kNoContent,
          "Unable to download payment manifest \"https://bobpay.test/\"."));

  downloader_->DownloadWebAppManifest(
      url::Origin::Create(test_url_), test_url_,
      base::BindOnce(&PaymentManifestDownloaderCSPTest::OnManifestDownload,
                     base::Unretained(this)));
}

// Download fails when CSP checker denies it.
TEST_F(PaymentManifestDownloaderCSPTest, PaymentMethodManifestCSPDenied) {
  const_csp_checker_ = std::make_unique<ConstCSPChecker>(/*allow=*/false);
  InitDownloader();

  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Content Security Policy denied the download of "
                         "payment manifest \"https://bobpay.test/\"."));

  downloader_->DownloadPaymentMethodManifest(
      url::Origin::Create(GURL("https://chromium.org")), test_url_,
      base::BindOnce(&PaymentManifestDownloaderCSPTest::OnManifestDownload,
                     base::Unretained(this)));
}

// Download fails when CSP checker denies it.
TEST_F(PaymentManifestDownloaderCSPTest, WebAppManifestCSPDenied) {
  const_csp_checker_ = std::make_unique<ConstCSPChecker>(/*allow=*/false);
  InitDownloader();

  EXPECT_CALL(*this, OnManifestDownload(
                         _, kNoContent,
                         "Content Security Policy denied the download of "
                         "payment manifest \"https://bobpay.test/\"."));

  downloader_->DownloadWebAppManifest(
      url::Origin::Create(test_url_), test_url_,
      base::BindOnce(&PaymentManifestDownloaderCSPTest::OnManifestDownload,
                     base::Unretained(this)));
}

}  // namespace payments
