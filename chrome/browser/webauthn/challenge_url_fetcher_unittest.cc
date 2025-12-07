// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/challenge_url_fetcher.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kUrl[] = "https://example.com/challenge_endpoint";
constexpr uint8_t kTestChallenge[] = {0, 1, 2,  3,  4,  5,  6,  7,
                                      8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint8_t kSmallChallenge[] = {0, 1, 2,  3,  4,  5,  6, 7,
                                       8, 9, 10, 11, 12, 13, 14};

constexpr char kChallengeContentType[] = "application/x-webauthn-challenge";
}  // namespace

class ChallengeUrlFetcherTest : public testing::Test {
 public:
  ChallengeUrlFetcherTest() = default;

  void SetUp() override {
    fetcher_ = std::make_unique<ChallengeUrlFetcher>(
        url_loader_factory_.GetSafeWeakWrapper());
  }

  ChallengeUrlFetcher* fetcher() { return fetcher_.get(); }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  base::expected<std::vector<uint8_t>,
                 ChallengeUrlFetcher::ChallengeNotAvailableReason>
  FetchChallengeAndWait() {
    base::test::TestFuture<void> future;
    fetcher()->FetchUrl(GURL(kUrl), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return fetcher()->GetChallenge();
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<ChallengeUrlFetcher> fetcher_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(ChallengeUrlFetcherTest, ChallengeFetchSuccess) {
  url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [this](const network::ResourceRequest& request) {
        EXPECT_EQ(request.redirect_mode, network::mojom::RedirectMode::kError);
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
        EXPECT_EQ(fetcher()->GetChallenge().error(),
                  ChallengeUrlFetcher::ChallengeNotAvailableReason::
                      kWaitingForChallenge);
        auto head = network::mojom::URLResponseHead::New();
        head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
        head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                 kChallengeContentType);
        std::string body(std::begin(kTestChallenge), std::end(kTestChallenge));
        url_loader_factory()->AddResponse(
            GURL(kUrl), std::move(head), body,
            network::URLLoaderCompletionStatus(net::Error::OK));
      }));

  auto result = FetchChallengeAndWait();
  std::vector<uint8_t> expected(std::begin(kTestChallenge),
                                std::end(kTestChallenge));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(expected, result.value());
}

TEST_F(ChallengeUrlFetcherTest, ChallengeFetchError) {
  url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [this](const network::ResourceRequest& request) {
        EXPECT_EQ(request.redirect_mode, network::mojom::RedirectMode::kError);
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
        EXPECT_EQ(fetcher()->GetChallenge().error(),
                  ChallengeUrlFetcher::ChallengeNotAvailableReason::
                      kWaitingForChallenge);
        auto head = network::mojom::URLResponseHead::New();
        head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
        head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                 kChallengeContentType);
        std::string body(std::begin(kTestChallenge), std::end(kTestChallenge));
        url_loader_factory()->AddResponse(
            GURL(kUrl), std::move(head), body,
            network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
      }));

  auto result = FetchChallengeAndWait();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      ChallengeUrlFetcher::ChallengeNotAvailableReason::kErrorFetchingChallenge,
      result.error());
}

TEST_F(ChallengeUrlFetcherTest, ChallengeFetchMissingHeader) {
  url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [this](const network::ResourceRequest& request) {
        EXPECT_EQ(request.redirect_mode, network::mojom::RedirectMode::kError);
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
        EXPECT_EQ(fetcher()->GetChallenge().error(),
                  ChallengeUrlFetcher::ChallengeNotAvailableReason::
                      kWaitingForChallenge);
        std::string body(std::begin(kTestChallenge), std::end(kTestChallenge));
        url_loader_factory()->AddResponse(kUrl, body);
      }));

  auto result = FetchChallengeAndWait();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      ChallengeUrlFetcher::ChallengeNotAvailableReason::kErrorFetchingChallenge,
      result.error());
}

TEST_F(ChallengeUrlFetcherTest, ChallengeTooSmall) {
  url_loader_factory()->SetInterceptor(base::BindLambdaForTesting(
      [this](const network::ResourceRequest& request) {
        EXPECT_EQ(request.redirect_mode, network::mojom::RedirectMode::kError);
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
        EXPECT_EQ(fetcher()->GetChallenge().error(),
                  ChallengeUrlFetcher::ChallengeNotAvailableReason::
                      kWaitingForChallenge);
        auto head = network::mojom::URLResponseHead::New();
        head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
        head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                                 kChallengeContentType);
        std::string body(std::begin(kSmallChallenge),
                         std::end(kSmallChallenge));
        url_loader_factory()->AddResponse(
            GURL(kUrl), std::move(head), body,
            network::URLLoaderCompletionStatus(net::Error::OK));
      }));

  auto result = FetchChallengeAndWait();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      ChallengeUrlFetcher::ChallengeNotAvailableReason::kErrorFetchingChallenge,
      result.error());
}
