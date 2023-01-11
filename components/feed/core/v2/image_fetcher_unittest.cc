// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/image_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"

using ::testing::HasSubstr;

namespace feed {
namespace {

class ImageFetcherTest : public testing::Test {
 public:
  ImageFetcherTest() = default;
  ImageFetcherTest(ImageFetcherTest&) = delete;
  ImageFetcherTest& operator=(const ImageFetcherTest&) = delete;
  ~ImageFetcherTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    image_fetcher_ = std::make_unique<ImageFetcher>(shared_url_loader_factory_);
  }

  ImageFetcher* image_fetcher() { return image_fetcher_.get(); }

  network::ResourceRequest Respond(const std::string& response_string,
                                   int net_status_code) {
    network::URLLoaderCompletionStatus status;

    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory_.GetPendingRequest(0);
    CHECK(pending_request);
    network::ResourceRequest resource_request = pending_request->request;
    auto head = network::mojom::URLResponseHead::New();
    if (net_status_code >= 0) {
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          "HTTP/1.1 " + base::NumberToString(net_status_code));
      status.decoded_body_length = response_string.length();
    } else {
      status.error_code = net_status_code;
    }

    test_factory_.AddResponse(pending_request->request.url, std::move(head),
                              response_string, status);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

 private:
  std::unique_ptr<ImageFetcher> image_fetcher_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ImageFetcherTest, SendRequestSendsValidRequest) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());
  network::ResourceRequest resource_request = Respond("", net::HTTP_OK);

  EXPECT_EQ(GURL("https://example.com"), resource_request.url);
  EXPECT_EQ("GET", resource_request.method);

  EXPECT_EQ(
      std::vector<base::Bucket>({{net::HTTP_OK, 1}}),
      histograms.GetAllSamples("ContentSuggestions.Feed.ImageFetchStatus"));
}

TEST_F(ImageFetcherTest, SendRequestReceivesNetError) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());
  Respond("", net::ERR_ACCESS_DENIED);

  EXPECT_EQ(
      std::vector<base::Bucket>({{net::ERR_ACCESS_DENIED, 1}}),
      histograms.GetAllSamples("ContentSuggestions.Feed.ImageFetchStatus"));
}

TEST_F(ImageFetcherTest, SendRequestValidResponse) {
  CallbackReceiver<NetworkResponse> receiver;
  image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());
  Respond("example_response", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_THAT(receiver.GetResult()->response_bytes,
              HasSubstr("example_response"));
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
}

TEST_F(ImageFetcherTest, SendSequentialRequestsValidResponses) {
  CallbackReceiver<NetworkResponse> receiver1;
  image_fetcher()->Fetch(GURL("https://example1.com"), receiver1.Bind());
  Respond("example1_response", net::HTTP_OK);

  CallbackReceiver<NetworkResponse> receiver2;
  image_fetcher()->Fetch(GURL("https://example2.com"), receiver2.Bind());
  Respond("example2_response", net::HTTP_OK);

  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(receiver1.GetResult()->response_bytes,
              HasSubstr("example1_response"));
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(receiver2.GetResult()->response_bytes,
              HasSubstr("example2_response"));
}

TEST_F(ImageFetcherTest, SendParallelRequestsValidResponses) {
  CallbackReceiver<NetworkResponse> receiver1;
  image_fetcher()->Fetch(GURL("https://example1.com"), receiver1.Bind());
  CallbackReceiver<NetworkResponse> receiver2;
  image_fetcher()->Fetch(GURL("https://example2.com"), receiver2.Bind());

  Respond("example1_response", net::HTTP_OK);
  Respond("example2_response", net::HTTP_OK);

  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(receiver1.GetResult()->response_bytes,
              HasSubstr("example1_response"));
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(receiver2.GetResult()->response_bytes,
              HasSubstr("example2_response"));
}

TEST_F(ImageFetcherTest, CancelRunsCallback) {
  CallbackReceiver<NetworkResponse> receiver;
  ImageFetchId id =
      image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());

  image_fetcher()->Cancel(id);
  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(std::string(), receiver.GetResult()->response_bytes);
  EXPECT_EQ(net::Error::ERR_ABORTED, receiver.GetResult()->status_code);
}

TEST_F(ImageFetcherTest, CancelThenRespond) {
  CallbackReceiver<NetworkResponse> receiver;
  ImageFetchId id =
      image_fetcher()->Fetch(GURL("https://example.com"), receiver.Bind());

  image_fetcher()->Cancel(id);
  Respond("example1_response", net::HTTP_OK);
  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(std::string(), receiver.GetResult()->response_bytes);
  EXPECT_EQ(net::Error::ERR_ABORTED, receiver.GetResult()->status_code);
}

TEST_F(ImageFetcherTest, CallbackCallsCancel) {
  // Ensure nothing terrible happens if cancel is called from the callback.
  ImageFetchId id;
  id = image_fetcher()->Fetch(
      GURL("https://example.com"),
      base::BindLambdaForTesting(
          [&](NetworkResponse response) { image_fetcher()->Cancel(id); }));

  image_fetcher()->Cancel(id);
}

}  // namespace
}  // namespace feed
