// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MultipartUploadRequestTest : public testing::Test {
 public:
  MultipartUploadRequestTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

class MockMultipartUploadRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadRequest()
      : MultipartUploadRequest(
            nullptr,
            GURL(),
            "",
            "",
            "",
            TRAFFIC_ANNOTATION_FOR_TESTS,
            base::DoNothing(),
            base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  MOCK_METHOD0(SendRequest, void());
};

TEST_F(MultipartUploadRequestTest, StringRequest_Failure) {
  base::HistogramTester histogram_tester;
  auto dummy_upload_url = GURL("https://google.com");

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      });

  test_url_loader_factory_.AddResponse(
      dummy_upload_url, network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED),
      "", network::URLLoaderCompletionStatus(net::OK));

  auto connector_request = MultipartUploadRequest::CreateStringRequest(
      test_shared_loader_factory_, dummy_upload_url, "metadata", "data",
      "DummySuffix", TRAFFIC_ANNOTATION_FOR_TESTS, std::move(callback),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

  request->Start();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.MultipartUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_UNAUTHORIZED,
      /*expected_bucket_count=*/1);
}

TEST_F(MultipartUploadRequestTest, StringRequest_Success) {
  base::HistogramTester histogram_tester;
  auto dummy_upload_url = GURL("https://google.com");

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      });

  test_url_loader_factory_.AddResponse(
      dummy_upload_url, network::CreateURLResponseHead(net::HTTP_OK), "",
      network::URLLoaderCompletionStatus(net::OK));

  auto connector_request = MultipartUploadRequest::CreateStringRequest(
      test_shared_loader_factory_, dummy_upload_url, "metadata", "data",
      "DummySuffix", TRAFFIC_ANNOTATION_FOR_TESTS, std::move(callback),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

  request->Start();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.MultipartUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

}  // namespace safe_browsing
