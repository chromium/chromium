// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_manifest_fetcher.h"

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/model/web_install_manifest_fetch_error.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using testing::Eq;

constexpr char kManifestUrl[] = "https://example.com/manifest.json";
constexpr char kRedirectUrl[] = "https://example.com/redirect";
constexpr char kRedirectTargetUrl[] = "https://example.com/redirected.json";

constexpr char kValidManifestJson[] = R"({
  "name": "Test App",
  "start_url": "/",
  "id": "/app"
})";

class WebInstallManifestFetcherTest : public ::testing::Test {
 public:
  WebInstallManifestFetcherTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

 protected:
  void AddResponse(const std::string& url,
                   const std::string& content,
                   std::optional<std::string> content_type,
                   net::HttpStatusCode http_status = net::HTTP_OK) {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(http_status);
    head->mime_type = "application/json";
    if (content_type) {
      head->headers->SetHeader("Content-Type", *content_type);
    } else {
      head->headers->RemoveHeader("Content-Type");
    }
    network::URLLoaderCompletionStatus status;
    test_factory_.AddResponse(GURL(url), std::move(head), content, status);
  }

  void AddRedirect(const std::string& url, const std::string& target) {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = GURL(target);
    redirect_info.status_code = 301;
    network::TestURLLoaderFactory::Redirects redirects;
    redirects.emplace_back(redirect_info,
                           network::mojom::URLResponseHead::New());
    test_factory_.AddResponse(
        GURL(url), network::CreateURLResponseHead(net::HTTP_OK),
        kValidManifestJson, network::URLLoaderCompletionStatus(),
        std::move(redirects));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(WebInstallManifestFetcherTest, FetchesManifestSuccessfully) {
  AddResponse(kManifestUrl, kValidManifestJson, "application/json");

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(), ValueIs(Eq(kValidManifestJson)));
}

struct ContentTypeTestCase {
  const char* name;
  std::optional<std::string> content_type;
  bool is_json;
};

class WebInstallManifestFetcherContentTypeTest
    : public WebInstallManifestFetcherTest,
      public testing::WithParamInterface<ContentTypeTestCase> {};

TEST_P(WebInstallManifestFetcherContentTypeTest, ValidatesContentTypeMetadata) {
  const auto& test_case = GetParam();
  // The derived MIME type is always JSON, even for missing or non-JSON HTTP
  // metadata, to ensure it cannot bypass Content-Type validation.
  AddResponse(kManifestUrl, kValidManifestJson, test_case.content_type);

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  if (test_case.is_json) {
    EXPECT_THAT(future.Take(), ValueIs(Eq(kValidManifestJson)));
  } else {
    EXPECT_THAT(future.Take(),
                ErrorIs(WebInstallManifestFetchError::kInvalidContentType));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebInstallManifestFetcherContentTypeTest,
    testing::Values(
        ContentTypeTestCase{"ApplicationJson", "application/json", true},
        ContentTypeTestCase{"TextJson", "text/json", true},
        ContentTypeTestCase{"ManifestJson", "application/manifest+json", true},
        ContentTypeTestCase{"VendorJson", "application/vnd.example+json", true},
        ContentTypeTestCase{"MixedCaseWithCharset",
                            "Application/JSON; charset=utf-8", true},
        ContentTypeTestCase{"JsonPrefixOnly", "application/jsonp", false},
        ContentTypeTestCase{"JsonSuffixNotTerminal",
                            "application/vnd.example+jsonx", false},
        ContentTypeTestCase{"JsonInParameterOnly",
                            "text/html; charset=application/json", false},
        ContentTypeTestCase{"TextPlain", "text/plain", false},
        ContentTypeTestCase{"TextHtml", "text/html", false},
        ContentTypeTestCase{"Missing", std::nullopt, false},
        ContentTypeTestCase{"Empty", "", false}),
    [](const testing::TestParamInfo<ContentTypeTestCase>& info) {
      return info.param.name;
    });

TEST_F(WebInstallManifestFetcherTest, FailsWhenResponseHeadersAreMissing) {
  auto head = network::mojom::URLResponseHead::New();
  head->mime_type = "application/json";
  test_factory_.AddResponse(GURL(kManifestUrl), std::move(head),
                            kValidManifestJson,
                            network::URLLoaderCompletionStatus());

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(),
              ErrorIs(WebInstallManifestFetchError::kInvalidContentType));
}

TEST_F(WebInstallManifestFetcherTest, FailsOnNetworkError) {
  // Simulate a real network error (connection refused). Use a minimal
  // response head — pairing HTTP_OK headers with a net error is contradictory
  // and causes the TestURLLoaderFactory to enter an inconsistent state.
  test_factory_.AddResponse(
      GURL(kManifestUrl), network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(),
              ErrorIs(WebInstallManifestFetchError::kDownloadFailed));
}

TEST_F(WebInstallManifestFetcherTest, FailsOnRedirect) {
  AddRedirect(kRedirectUrl, kRedirectTargetUrl);

  WebInstallManifestFetcher fetcher(GURL(kRedirectUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(),
              ErrorIs(WebInstallManifestFetchError::kRedirected));
}

TEST_F(WebInstallManifestFetcherTest, FailsOn404) {
  test_factory_.AddResponse(std::string(kManifestUrl), "", net::HTTP_NOT_FOUND);

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(),
              ErrorIs(WebInstallManifestFetchError::kDownloadFailed));
}

TEST_F(WebInstallManifestFetcherTest, FetchesEmptyManifest) {
  AddResponse(kManifestUrl, "", "application/json");

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  // An empty string is still a successful download — parsing failures
  // will be caught in a later stage.
  EXPECT_THAT(future.Take(), ValueIs(Eq("")));
}

TEST_F(WebInstallManifestFetcherTest, FailsWhenResponseExceedsMaxLength) {
  // Override the max length to a small value so we don't need to allocate a
  // 5MB+ string in tests.
  auto scoped_override =
      WebInstallManifestFetcher::SetMaxManifestLengthForTesting(10);

  AddResponse(kManifestUrl, std::string(100, 'x'), "application/json");

  WebInstallManifestFetcher fetcher(GURL(kManifestUrl),
                                    shared_url_loader_factory_);
  base::test::TestFuture<
      base::expected<std::string, WebInstallManifestFetchError>>
      future;
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Take(),
              ErrorIs(WebInstallManifestFetchError::kDownloadFailed));
}

TEST_F(WebInstallManifestFetcherTest, DestroyedMidFlightDoesNotCrash) {
  // Set up a pending request that won't auto-complete.
  test_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Don't respond — leave the request pending.
      }));

  auto fetcher = std::make_unique<WebInstallManifestFetcher>(
      GURL(kManifestUrl), shared_url_loader_factory_);

  bool callback_called = false;
  fetcher->Fetch(base::BindLambdaForTesting(
      [&](base::expected<std::string, WebInstallManifestFetchError>) {
        callback_called = true;
      }));

  // Destroy the fetcher while the request is still in flight.
  fetcher.reset();

  // The callback should not have been invoked after destruction.
  EXPECT_FALSE(callback_called);
}

}  // namespace
}  // namespace web_app
