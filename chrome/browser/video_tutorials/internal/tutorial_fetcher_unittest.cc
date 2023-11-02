// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_fetcher.h"

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
const char kGoogleApiKey[] = "api-key";
const char kVideoTutorialsMessage[] = R"pb(
  tutorial_groups {
    locale: "en"
    tutorial {
      feature: "1"
      title: "feature1"
      video_url: "https://some_url.com/video.mp4"
      poster_url: "https://some_url.com/poster.jpg"
      caption_url: "https://some_url.com/caption.vtt"
    }
  })pb";
}  // namespace

namespace video_tutorials {
class TutorialFetcherTest : public testing::Test {
 public:
  TutorialFetcherTest();
  ~TutorialFetcherTest() override = default;

  TutorialFetcherTest(const TutorialFetcherTest& other) = delete;
  TutorialFetcherTest& operator=(const TutorialFetcherTest& other) = delete;

  void SetUp() override;

  std::unique_ptr<TutorialFetcher> CreateFetcher();

  bool RunFetcherWithNetError(net::Error net_error);
  bool RunFetcherWithHttpError(net::HttpStatusCode http_error);
  bool RunFetcherWithData(const std::string& response_data,
                          std::string* data_received);

  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);
  void RespondWithData(const std::string& data);

  network::ResourceRequest last_resource_request() const {
    return last_resource_request_;
  }

 private:
  TutorialFetcher::FinishedCallback StoreResult();
  bool RunFetcher(base::OnceCallback<void(void)> respond_callback,
                  std::string* data_received);

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  std::unique_ptr<std::string> last_data_;
  bool last_status_ = false;
  network::ResourceRequest last_resource_request_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO,
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TutorialFetcherTest::TutorialFetcherTest()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

void TutorialFetcherTest::SetUp() {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(request.url.is_valid() && !request.url.is_empty());
        last_resource_request_ = request;
      }));
}

TutorialFetcher::FinishedCallback TutorialFetcherTest::StoreResult() {
  return base::BindLambdaForTesting(
      [&](bool success, std::unique_ptr<std::string> data) {
        last_status_ = success;
        last_data_ = std::move(data);
      });
}

std::unique_ptr<TutorialFetcher> TutorialFetcherTest::CreateFetcher() {
  std::unique_ptr<TutorialFetcher> fetcher =
      TutorialFetcher::Create(GURL(kServerURL), "", "", kGoogleApiKey, "", "",
                              test_shared_url_loader_factory_);
  return fetcher;
}

bool TutorialFetcherTest::RunFetcherWithNetError(net::Error net_error) {
  std::string data_received;
  bool status =
      RunFetcher(base::BindOnce(&TutorialFetcherTest::RespondWithNetError,
                                base::Unretained(this), net_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());
  return status;
}

bool TutorialFetcherTest::RunFetcherWithHttpError(
    net::HttpStatusCode http_error) {
  std::string data_received;
  bool status =
      RunFetcher(base::BindOnce(&TutorialFetcherTest::RespondWithHttpError,
                                base::Unretained(this), http_error),
                 &data_received);
  EXPECT_TRUE(data_received.empty());

  return status;
}

bool TutorialFetcherTest::RunFetcherWithData(const std::string& response_data,
                                             std::string* data_received) {
  bool success =
      RunFetcher(base::BindOnce(&TutorialFetcherTest::RespondWithData,
                                base::Unretained(this), response_data),
                 data_received);
  return success;
}

void TutorialFetcherTest::RespondWithNetError(int net_error) {
  network::URLLoaderCompletionStatus completion_status(net_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url, completion_status,
      network::mojom::URLResponseHead::New(), std::string(),
      network::TestURLLoaderFactory::kMostRecentMatch);
}

void TutorialFetcherTest::RespondWithHttpError(net::HttpStatusCode http_error) {
  auto url_response_head = network::CreateURLResponseHead(http_error);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url, network::URLLoaderCompletionStatus(net::OK),
      std::move(url_response_head), std::string(),
      network::TestURLLoaderFactory::kMostRecentMatch);
}

void TutorialFetcherTest::RespondWithData(const std::string& data) {
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      last_resource_request_.url.spec(), data, net::HTTP_OK,
      network::TestURLLoaderFactory::kMostRecentMatch);
}

bool TutorialFetcherTest::RunFetcher(
    base::OnceCallback<void(void)> respond_callback,
    std::string* data_received) {
  auto fetcher = CreateFetcher();
  fetcher->StartFetchForTutorials(StoreResult());
  std::move(respond_callback).Run();
  task_environment_.RunUntilIdle();
  if (last_data_)
    *data_received = *last_data_;
  return last_status_;
}

// Tests that net errors will result in failed status.
TEST_F(TutorialFetcherTest, NetErrors) {
  EXPECT_EQ(false, RunFetcherWithNetError(net::ERR_BLOCKED_BY_ADMINISTRATOR));
}

// Tests that http errors will result in failed status.
TEST_F(TutorialFetcherTest, HttpErrors) {
  EXPECT_EQ(false, RunFetcherWithHttpError(net::HTTP_NOT_FOUND));
}

// Tests that empty responses are handled properly.
TEST_F(TutorialFetcherTest, EmptyResponse) {
  std::string data;
  EXPECT_EQ(true, RunFetcherWithData("", &data));
  EXPECT_TRUE(data.empty());
}

// Tests that a susscess response is received properly.
TEST_F(TutorialFetcherTest, Success) {
  std::string data;
  EXPECT_EQ(true, RunFetcherWithData(kVideoTutorialsMessage, &data));
  EXPECT_EQ(kVideoTutorialsMessage, data);

  EXPECT_EQ(last_resource_request().url.spec(),
            "https://www.test.com/?country_code=");
}

}  // namespace video_tutorials
