// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_data_fetcher.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/base/data_url.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kImageURL[] = "http://www.example.com/image";
const char kURLResponseData[] = "EncodedImageData";

const char kTestUmaClientName[] = "TestUmaClient";
const char kHistogramName[] = "ImageFetcher.RequestStatusCode";

}  // namespace

namespace image_fetcher {

class ImageDataFetcherTest : public testing::Test {
 public:
  ImageDataFetcherTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        image_data_fetcher_(shared_factory_) {}

  ImageDataFetcherTest(const ImageDataFetcherTest&) = delete;
  ImageDataFetcherTest& operator=(const ImageDataFetcherTest&) = delete;

  ~ImageDataFetcherTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  MOCK_METHOD2(OnImageDataFetched,
               void(const std::string&, const RequestMetadata&));

  MOCK_METHOD2(OnImageDataFetchedFailedRequest,
               void(const std::string&, const RequestMetadata&));

  MOCK_METHOD2(OnImageDataFetchedMultipleRequests,
               void(const std::string&, const RequestMetadata&));

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  ImageDataFetcher image_data_fetcher_;
};

TEST_F(ImageDataFetcherTest, FetchImageData) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_OK;
  EXPECT_CALL(*this, OnImageDataFetched(content, expected_metadata));

  // Check to make sure the request is pending with proper flags, and
  // provide a response.
  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL, &pending_request));
  EXPECT_EQ(pending_request->credentials_mode,
            network::mojom::CredentialsMode::kOmit);

  auto head = network::mojom::URLResponseHead::New();
  std::string raw_header =
      "HTTP/1.1 200 OK\n"
      "Content-type: image/png\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_header));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head),
                                       content, status);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectBucketCount(std::string(kHistogramName), 200, 1);
}

TEST_F(ImageDataFetcherTest, FetchImageDataWithDataUrl) {
  std::string data_url =
      "data:image/png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQYlWNk+M/"
      "wn4GBgYGJAQoAHhgCAh6X4CYAAAAASUVORK5CYII=";

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = "image/png";
  std::string mime_type, expected, charset;
  EXPECT_TRUE(
      net::DataURL::Parse(GURL(data_url), &mime_type, &charset, &expected));
  EXPECT_CALL(*this, OnImageDataFetched(expected, expected_metadata));

  image_data_fetcher_.FetchImageData(
      GURL(data_url),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));
}

TEST_F(ImageDataFetcherTest, FetchImageDataTrafficAnnotationOnly) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS, false);

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_OK;
  EXPECT_CALL(*this, OnImageDataFetched(content, expected_metadata));

  // Check to make sure the request is pending with proper flags, and
  // provide a response.
  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL, &pending_request));
  EXPECT_EQ(pending_request->credentials_mode,
            network::mojom::CredentialsMode::kOmit);

  auto head = network::mojom::URLResponseHead::New();
  std::string raw_header =
      "HTTP/1.1 200 OK\n"
      "Content-type: image/png\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_header));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head),
                                       content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageDataWithCookies) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName),
      true);

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_OK;
  EXPECT_CALL(*this, OnImageDataFetched(content, expected_metadata));

  // Check to make sure the request is pending with proper flags, and
  // provide a response.
  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL, &pending_request));
  EXPECT_EQ(pending_request->credentials_mode,
            network::mojom::CredentialsMode::kInclude);

  auto head = network::mojom::URLResponseHead::New();
  std::string raw_header =
      "HTTP/1.1 200 OK\n"
      "Content-type: image/png\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_header));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head),
                                       content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_NotFound) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_NOT_FOUND;
  // For 404, expect an empty result even though correct image data is sent.
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  auto head = network::mojom::URLResponseHead::New();
  std::string raw_header =
      "HTTP/1.1 404 Not Found\n"
      "Content-type: image/png\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_header));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head),
                                       content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_WithContentLocation) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_NOT_FOUND;
  expected_metadata.content_location_header = "http://test-location/image.png";
  // For 404, expect an empty result even though correct image data is sent.
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  auto head = network::mojom::URLResponseHead::New();
  std::string raw_header =
      "HTTP/1.1 404 Not Found\n"
      "Content-type: image/png\n"
      "Content-location: http://test-location/image.png\n\n";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_header));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head),
                                       content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_FailedRequest) {
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedFailedRequest,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  RequestMetadata expected_metadata;
  expected_metadata.http_response_code = RequestMetadata::RESPONSE_CODE_INVALID;
  EXPECT_CALL(
      *this, OnImageDataFetchedFailedRequest(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  auto head = network::mojom::URLResponseHead::New();
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_INVALID_URL;
  test_url_loader_factory_.AddResponse(GURL(kImageURL), std::move(head), "",
                                       status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_MultipleRequests) {
  EXPECT_CALL(*this, OnImageDataFetchedMultipleRequests(testing::_, testing::_))
      .Times(2);

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedMultipleRequests,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedMultipleRequests,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  // Multiple calls to FetchImageData for the same URL will result in
  // multiple URLFetchers being created.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(kImageURL, "");
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_CancelFetchIfImageExceedsMaxSize) {
  const int64_t kMaxDownloadBytes = 1024;
  std::string oversize_download(kMaxDownloadBytes + 1, '#');

  image_data_fetcher_.SetImageDownloadLimit(kMaxDownloadBytes);
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  // Fetching an oversized image will behave like any other failed request.
  // There will be exactly one call to OnImageDataFetched containing a response
  // code that would be impossible for a completed fetch.
  RequestMetadata expected_metadata;
  expected_metadata.http_response_code = RequestMetadata::RESPONSE_CODE_INVALID;
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));
  test_url_loader_factory_.AddResponse(kImageURL, oversize_download);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, DeleteFromCallback) {
  // Test to make sure that deleting an ImageDataFetcher from the callback
  // passed to its FetchImageData() does not crash.
  auto heap_fetcher = std::make_unique<ImageDataFetcher>(shared_factory_);
  heap_fetcher->FetchImageData(
      GURL(kImageURL),
      base::BindLambdaForTesting(
          [&](const std::string&, const RequestMetadata&) {
            heap_fetcher = nullptr;
          }),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kTestUmaClientName));

  test_url_loader_factory_.AddResponse(kImageURL, "");
  base::RunLoop().RunUntilIdle();
}

}  // namespace image_fetcher
