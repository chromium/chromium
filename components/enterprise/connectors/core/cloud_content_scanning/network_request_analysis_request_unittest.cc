// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_analysis_request.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/uploader_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

class FakeChunkedDataPipeGetter : public network::mojom::ChunkedDataPipeGetter {
 public:
  FakeChunkedDataPipeGetter() = default;
  ~FakeChunkedDataPipeGetter() override = default;

  mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
  GetPendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // network::mojom::ChunkedDataPipeGetter:
  void GetSize(GetSizeCallback callback) override {}
  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override {}

 private:
  mojo::Receiver<network::mojom::ChunkedDataPipeGetter> receiver_{this};
};

CloudAnalysisSettings CreateCloudSettings() {
  return CloudAnalysisSettings();
}

BinaryUploadRequest::BrowserPolicyConnectorGetter NullPolicyConnectorGetter() {
  return base::BindRepeating(
      []() -> policy::BrowserPolicyConnector* { return nullptr; });
}

// Runs `body` through a request and returns the resulting scan result and the
// size computed for the body.
std::pair<ScanRequestUploadResult, uint64_t> GetResultAndSize(
    scoped_refptr<network::ResourceRequestBody> body) {
  NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                        base::DoNothing(),
                                        NullPolicyConnectorGetter());

  base::RunLoop run_loop;
  ScanRequestUploadResult out_result = ScanRequestUploadResult::kUnknown;
  uint64_t out_size = 0;
  request.GetRequestData(base::BindLambdaForTesting(
      [&](ScanRequestUploadResult result, BinaryUploadRequest::Data data) {
        out_result = result;
        out_size = data.size;
        run_loop.Quit();
      }));
  run_loop.Run();
  return {out_result, out_size};
}

}  // namespace

class NetworkRequestAnalysisRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NetworkRequestAnalysisRequestTest, EmptyRequestBody) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                        base::DoNothing(),
                                        NullPolicyConnectorGetter());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &body](ScanRequestUploadResult result,
                         BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, 0u);
        EXPECT_EQ(data.request_body, body);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(NetworkRequestAnalysisRequestTest, BytesElements) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  std::string part1 = "hello ";
  std::string part2 = "world!";
  body->AppendCopyOfBytes(base::as_byte_span(part1));
  body->AppendCopyOfBytes(base::as_byte_span(part2));

  NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                        base::DoNothing(),
                                        NullPolicyConnectorGetter());

  base::RunLoop run_loop;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &body, &part1, &part2](ScanRequestUploadResult result,
                                         BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, part1.size() + part2.size());
        EXPECT_EQ(data.request_body, body);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(NetworkRequestAnalysisRequestTest, MaxSizeEnforcementLegacyLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableNewUploadSizeLimit);

  uint64_t too_large_size = BinaryUploadService::kMaxUploadSizeBytes + 1;
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("nonexistent_file")),
                        0, too_large_size, base::Time());

  auto [result, size] = GetResultAndSize(body);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(size, too_large_size);
}

TEST_F(NetworkRequestAnalysisRequestTest, MaxSizeEnforcementNewLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableNewUploadSizeLimit, {{"max_file_size_mb", "250"}});

  uint64_t new_limit = 250 * 1024 * 1024;

  // Over the legacy 50MB limit but under the new one, so it's accepted.
  {
    auto body = base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("nonexistent_file")),
                          0, BinaryUploadService::kMaxUploadSizeBytes + 1,
                          base::Time());

    auto [result, size] = GetResultAndSize(body);
    EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
    EXPECT_EQ(size, BinaryUploadService::kMaxUploadSizeBytes + 1);
  }

  // Over the new limit, so it's rejected.
  {
    auto body = base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("nonexistent_file")),
                          0, new_limit + 1, base::Time());

    auto [result, size] = GetResultAndSize(body);
    EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
    EXPECT_EQ(size, new_limit + 1);
  }
}

TEST_F(NetworkRequestAnalysisRequestTest, SizeOverflowIsTooLarge) {
  // Two elements that each nearly fill a uint64_t. Summing them overflows,
  // which must saturate to "too large" rather than wrapping to a small size.
  uint64_t huge = std::numeric_limits<uint64_t>::max() - 1;
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("nonexistent_file")),
                        0, huge, base::Time());
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("nonexistent_file2")),
                        0, huge, base::Time());

  auto [result, size] = GetResultAndSize(body);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(size, std::numeric_limits<uint64_t>::max());
}

TEST_F(NetworkRequestAnalysisRequestTest, FileElements) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.txt");
  std::string file_contents = "0123456789abcdef";
  ASSERT_TRUE(base::WriteFile(file_path, file_contents));

  // Whole file (length == max).
  {
    auto body = base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(file_path, 0, std::numeric_limits<uint64_t>::max(),
                          base::Time());

    NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                          base::DoNothing(),
                                          NullPolicyConnectorGetter());

    base::RunLoop run_loop;
    request.GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &file_contents](ScanRequestUploadResult result,
                                    BinaryUploadRequest::Data data) {
          EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
          EXPECT_EQ(data.size, file_contents.size());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Sliced file (with offset and length).
  {
    auto body = base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(file_path, /*offset=*/4, /*length=*/6, base::Time());

    NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                          base::DoNothing(),
                                          NullPolicyConnectorGetter());

    base::RunLoop run_loop;
    request.GetRequestData(
        base::BindLambdaForTesting([&run_loop](ScanRequestUploadResult result,
                                               BinaryUploadRequest::Data data) {
          EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
          EXPECT_EQ(data.size, 6u);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Missing file with unknown length (max) -> size is indeterminate.
  {
    auto body = base::MakeRefCounted<network::ResourceRequestBody>();
    body->AppendFileRange(temp_dir.GetPath().AppendASCII("does_not_exist.txt"),
                          0, std::numeric_limits<uint64_t>::max(),
                          base::Time());

    auto [result, size] = GetResultAndSize(body);
    EXPECT_EQ(result, ScanRequestUploadResult::kUnknown);
    EXPECT_EQ(size, 0u);
  }
}

TEST_F(NetworkRequestAnalysisRequestTest, DataPipeElementSuccess) {
  std::string pipe_data = "data pipe contents for network request scanning";
  mojo::PendingRemote<network::mojom::DataPipeGetter> pending_remote;
  network::TestDataPipeGetter data_pipe_getter(
      pipe_data, pending_remote.InitWithNewPipeAndPassReceiver());

  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendDataPipe(std::move(pending_remote));

  NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                        base::DoNothing(),
                                        NullPolicyConnectorGetter());

  base::RunLoop run_loop;
  BinaryUploadRequest::Data received_data;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &received_data, &pipe_data](ScanRequestUploadResult result,
                                              BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, pipe_data.size());
        received_data = std::move(data);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify the request body data pipe can still be read in full afterwards.
  ASSERT_NE(received_data.request_body, nullptr);
  EXPECT_EQ(GetBodyFromResourceRequestBody(*received_data.request_body),
            pipe_data);
}

TEST_F(NetworkRequestAnalysisRequestTest, DataPipeElementError) {
  std::string pipe_data = "data pipe contents";
  mojo::PendingRemote<network::mojom::DataPipeGetter> pending_remote;
  network::TestDataPipeGetter data_pipe_getter(
      pipe_data, pending_remote.InitWithNewPipeAndPassReceiver());
  data_pipe_getter.set_start_error(net::ERR_FAILED);

  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendCopyOfBytes(base::as_byte_span(std::string("prefix")));
  body->AppendDataPipe(std::move(pending_remote));

  // When an element's size can't be determined the body size is unknown, so
  // the request can't be size-checked and reports `kUnknown` with size 0.
  auto [result, size] = GetResultAndSize(body);
  EXPECT_EQ(result, ScanRequestUploadResult::kUnknown);
  EXPECT_EQ(size, 0u);
}

TEST_F(NetworkRequestAnalysisRequestTest, ChunkedDataPipeElement) {
  FakeChunkedDataPipeGetter chunked_getter;
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->SetToChunkedDataPipe(chunked_getter.GetPendingRemote(),
                             network::ResourceRequestBody::ReadOnlyOnce(false));

  // A chunked body has no length up front, so it can't be bounded against the
  // upload limit and is reported as too large. The size is 0 because none is
  // available, not because the body is empty.
  auto [result, size] = GetResultAndSize(body);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(size, 0u);
}

TEST_F(NetworkRequestAnalysisRequestTest, MixedElementsAndMultipleCalls) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("part.txt");
  std::string file_part = "file_data_123";
  ASSERT_TRUE(base::WriteFile(file_path, file_part));

  std::string bytes_part = "bytes_data_456";
  std::string pipe_part = "pipe_data_789";

  mojo::PendingRemote<network::mojom::DataPipeGetter> pending_remote;
  network::TestDataPipeGetter data_pipe_getter(
      pipe_part, pending_remote.InitWithNewPipeAndPassReceiver());

  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendCopyOfBytes(base::as_byte_span(bytes_part));
  body->AppendFileRange(file_path, 0, std::numeric_limits<uint64_t>::max(),
                        base::Time());
  body->AppendDataPipe(std::move(pending_remote));

  NetworkRequestAnalysisRequest request(CreateCloudSettings(), body,
                                        base::DoNothing(),
                                        NullPolicyConnectorGetter());

  uint64_t expected_size =
      bytes_part.size() + file_part.size() + pipe_part.size();

  // Call GetRequestData twice concurrently.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  BinaryUploadRequest::Data data1;
  BinaryUploadRequest::Data data2;

  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop1, &data1, expected_size](ScanRequestUploadResult result,
                                          BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, expected_size);
        data1 = std::move(data);
        run_loop1.Quit();
      }));
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop2, &data2, expected_size](ScanRequestUploadResult result,
                                          BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, expected_size);
        data2 = std::move(data);
        run_loop2.Quit();
      }));

  run_loop1.Run();
  run_loop2.Run();

  EXPECT_EQ(data1.size, data2.size);
  EXPECT_EQ(data1.request_body, data2.request_body);

  // Call GetRequestData a third time after result is cached.
  base::RunLoop run_loop3;
  request.GetRequestData(base::BindLambdaForTesting(
      [&run_loop3, expected_size](ScanRequestUploadResult result,
                                  BinaryUploadRequest::Data data) {
        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, expected_size);
        run_loop3.Quit();
      }));
  run_loop3.Run();
}

// A `DataCallback` may destroy the request synchronously;
// `CloudBinaryUploadServiceBase::OnGetRequestData` does so via
// `FinishAndCleanupRequest` for any non-`kSuccess` result. Remaining pending
// callbacks must still run safely without touching the destroyed request.
// Regression test for a use-after-free; fails under ASAN without the fix.
TEST_F(NetworkRequestAnalysisRequestTest, RequestDestroyedByFirstCallback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // An unreadable file with an unknown length resolves asynchronously to
  // `kUnknown`, which is the result that makes production code destroy the
  // request from inside the callback.
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(temp_dir.GetPath().AppendASCII("does_not_exist.txt"), 0,
                        std::numeric_limits<uint64_t>::max(), base::Time());

  auto request = std::make_unique<NetworkRequestAnalysisRequest>(
      CreateCloudSettings(), body, base::DoNothing(),
      NullPolicyConnectorGetter());

  base::RunLoop run_loop;
  int callback_count = 0;
  ScanRequestUploadResult first_result = ScanRequestUploadResult::kSuccess;
  ScanRequestUploadResult second_result = ScanRequestUploadResult::kSuccess;
  uint64_t second_size = 12345;

  // Both callbacks are queued before the size resolves, so they're run back to
  // back by the same `CacheResultAndData` call.
  request->GetRequestData(base::BindLambdaForTesting(
      [&](ScanRequestUploadResult result, BinaryUploadRequest::Data data) {
        ++callback_count;
        first_result = result;
        request.reset();
      }));
  request->GetRequestData(base::BindLambdaForTesting(
      [&](ScanRequestUploadResult result, BinaryUploadRequest::Data data) {
        ++callback_count;
        second_result = result;
        second_size = data.size;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_EQ(callback_count, 2);
  EXPECT_EQ(first_result, ScanRequestUploadResult::kUnknown);
  // The second callback still receives the correct values even though the
  // request was destroyed by the first.
  EXPECT_EQ(second_result, ScanRequestUploadResult::kUnknown);
  EXPECT_EQ(second_size, 0u);
  EXPECT_EQ(request, nullptr);
}

}  // namespace enterprise_connectors
