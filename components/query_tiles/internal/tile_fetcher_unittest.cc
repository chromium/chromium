// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_fetcher.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {
const char kServerURL[] = "https://www.test.com";
const char kAcceptLanguages[] = "en-US,en;q=0.5";
const char kGoogleApiKey[] = "api-key";
const char kTileGroupsMessage[] = R"pb(
  tile_groups {
    locale: "en"
    tiles {
      tile_id: "news"
      query_string: "News"
      display_text: "News"
      accessibility_text: "News"
      is_top_level: 1
      sub_tile_ids: [ "news_india" ]
    }
    tiles {
      tile_id: "news_india"
      query_string: "India news"
      display_text: "India"
      accessibility_text: "India"
      is_top_level: 0
    }
  })pb";
}  // namespace

namespace query_tiles {
class TileFetcherTest : public testing::Test {
 public:
  TileFetcherTest();
  ~TileFetcherTest() override = default;

  TileFetcherTest(const TileFetcherTest& other) = delete;
  TileFetcherTest& operator=(const TileFetcherTest& other) = delete;

  void SetUp() override;

  std::unique_ptr<TileFetcher> CreateFetcher();

  TileInfoRequestStatus RunFetcherWithNetError(net::Error net_error);
  TileInfoRequestStatus RunFetcherWithHttpError(net::HttpStatusCode http_error);
  TileInfoRequestStatus RunFetcherWithData(const std::string& response_data,
                                           std::string* data_received);

  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);
  void RespondWithData(const std::string& data);

  network::ResourceRequest last_resource_request() const {
    return last_resource_request_;
  }

 private:
  TileFetcher::FinishedCallback StoreResult();
  TileInfoRequestStatus RunFetcher(
      base::OnceCallback<void(void)> respond_callback,
      std::string* data_received);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  TileInfoRequestStatus last_status_;
  std::unique_ptr<std::string> last_data_;
  network::ResourceRequest last_resource_request_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TileFetcherTest::TileFetcherTest()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

void TileFetcherTest::SetUp() {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
        last_resource_request_ = request;
      }));
}

TileFetcher::FinishedCallback TileFetcherTest::StoreResult() {
  return base::BindLambdaForTesting(
      [&](TileInfoRequestStatus status, std::unique_ptr<std::string> data) {
        last_status_ = status;
        last_data_ = std::move(data);
      });
}

std::unique_ptr<TileFetcher> TileFetcherTest::CreateFetcher() {
  std::unique_ptr<TileFetcher> fetcher = TileFetcher::Create(
      GURL(kServerURL), "US", kAcceptLanguages, kGoogleApiKey, "", "",
      test_shared_url_loader_factory_);
  return fetcher;
}

TileInfoRequestStatus TileFetcherTest::RunFetcherWithNetError(
    net::Error net_error) {
  std::string data_received;
  TileInfoRequestStatus status =
      RunFetcher(base::BindOnce(&TileFetcherTest::RespondWithNetError,
                                base::Unretained(this), net_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

TileInfoRequestStatus TileFetcherTest::RunFetcherWithHttpError(
    net::HttpStatusCode http_error) {
  std::string data_received;
  TileInfoRequestStatus status =
      RunFetcher(base::BindOnce(&TileFetcherTest::RespondWithHttpError,
                                base::Unretained(this), http_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());

  return status;
}

TileInfoRequestStatus TileFetcherTest::RunFetcherWithData(
    const std::string& response_data,
    std::string* data_received) {
  TileInfoRequestStatus status =
      RunFetcher(base::BindOnce(&TileFetcherTest::RespondWithData,
                                base::Unretained(this), response_data),
                 data_received);
  return status;
}

void TileFetcherTest::RespondWithNetError(int net_error) {
  network::URLLoaderCompletionStatus completion_status(net_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url, completion_status,
      network::mojom::URLResponseHead::New(), std::string(),
      network::TestURLLoaderFactory::kMostRecentMatch);
}

void TileFetcherTest::RespondWithHttpError(net::HttpStatusCode http_error) {
  auto url_response_head = network::CreateURLResponseHead(http_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url, network::URLLoaderCompletionStatus(net::OK),
      std::move(url_response_head), std::string(),
      network::TestURLLoaderFactory::kMostRecentMatch);
}

void TileFetcherTest::RespondWithData(const std::string& data) {
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url.spec(), data, net::HTTP_OK,
      network::TestURLLoaderFactory::kMostRecentMatch);
}

TileInfoRequestStatus TileFetcherTest::RunFetcher(
    base::OnceCallback<void(void)> respond_callback,
    std::string* data_received) {
  auto fetcher = CreateFetcher();
  fetcher->StartFetchForTiles(StoreResult());
  std::move(respond_callback).Run();
  task_environment_.RunUntilIdle();
  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

// Tests that net errors will result in failed status.
TEST_F(TileFetcherTest, NetErrors) {
  EXPECT_EQ(TileInfoRequestStatus::kShouldSuspend,
            RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_NETWORK_CHANGED));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_CLOSED));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithNetError(net::ERR_CONNECTION_REFUSED));
}

// Tests that http errors will result in failed status.
TEST_F(TileFetcherTest, HttpErrors) {
  EXPECT_EQ(TileInfoRequestStatus::kShouldSuspend,
            RunFetcherWithHttpError(net::HTTP_FORBIDDEN));
  EXPECT_EQ(TileInfoRequestStatus::kShouldSuspend,
            RunFetcherWithHttpError(net::HTTP_NOT_IMPLEMENTED));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_CONFLICT));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_BAD_GATEWAY));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_SERVICE_UNAVAILABLE));
  EXPECT_EQ(TileInfoRequestStatus::kFailure,
            RunFetcherWithHttpError(net::HTTP_GATEWAY_TIMEOUT));
}

// Tests that empty responses are handled properly.
TEST_F(TileFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(TileInfoRequestStatus::kSuccess, RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

// Tests that a susscess response is received properly.
TEST_F(TileFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(TileInfoRequestStatus::kSuccess,
            RunFetcherWithData(kTileGroupsMessage, &data));
  EXPECT_EQ(kTileGroupsMessage, data);

  EXPECT_EQ(last_resource_request().url.spec(),
            "https://www.test.com/?country_code=US");
}

}  // namespace query_tiles
