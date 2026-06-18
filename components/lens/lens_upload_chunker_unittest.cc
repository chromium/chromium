// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_upload_chunker.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

#if !BUILDFLAG(IS_ANDROID)

namespace lens {

namespace {

class FakeUploadChunkerDelegate : public LensUploadChunker::Delegate {
 public:
  FakeUploadChunkerDelegate() = default;
  ~FakeUploadChunkerDelegate() override = default;

  // Delegate overrides:
  void UploadChunk(
      const lens::LensOverlayUploadChunkRequest& request,
      base::RepeatingCallback<void(uint64_t position, uint64_t total)>
          progress_callback,
      base::OnceCallback<
          void(std::unique_ptr<endpoint_fetcher::EndpointResponse>)>
          completion_callback) override {
    uploaded_requests_.push_back(request);
    progress_callbacks_[request.chunk_id()] = progress_callback;
    completion_callbacks_[request.chunk_id()] = std::move(completion_callback);
  }

  void OnPageContentPayloadReady(const lens::LensOverlayRequestId& request_id,
                                 lens::Payload payload) override {
    payload_ready_called_ = true;
    last_payload_ = std::move(payload);
  }

  void OnChunkUploadError(LensUploadChunker::ErrorType error_type) override {
    error_called_ = true;
    last_error_type_ = error_type;
  }

  void OnUploadProgress(uint64_t position, uint64_t total) override {
    progress_called_ = true;
    last_position_ = position;
    last_total_ = total;
  }

  lens::LensOverlayClientContext GetClientContext() override {
    lens::LensOverlayClientContext context;
    context.set_platform(lens::PLATFORM_WEB);
    return context;
  }

  // Helpers to simulate responses.
  void SimulateChunkResponse(int64_t chunk_id,
                             google_apis::ApiErrorCode status_code,
                             const std::string& response_body) {
    auto response = std::make_unique<endpoint_fetcher::EndpointResponse>();
    response->http_status_code = status_code;
    response->response = response_body;
    std::move(completion_callbacks_[chunk_id]).Run(std::move(response));
  }

  void SimulateChunkProgress(int64_t chunk_id,
                             uint64_t position,
                             uint64_t total) {
    progress_callbacks_[chunk_id].Run(position, total);
  }

  std::vector<lens::LensOverlayUploadChunkRequest> uploaded_requests_;
  std::map<int64_t,
           base::RepeatingCallback<void(uint64_t position, uint64_t total)>>
      progress_callbacks_;
  std::map<int64_t,
           base::OnceCallback<void(
               std::unique_ptr<endpoint_fetcher::EndpointResponse>)>>
      completion_callbacks_;

  bool payload_ready_called_ = false;
  lens::Payload last_payload_;

  bool error_called_ = false;
  LensUploadChunker::ErrorType last_error_type_;

  bool progress_called_ = false;
  uint64_t last_position_ = 0;
  uint64_t last_total_ = 0;
};

class LensUploadChunkerTest : public testing::Test {
 public:
  LensUploadChunkerTest() = default;
  ~LensUploadChunkerTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayUploadChunking,
          {{"chunk-size-bytes", "3"}, {"upload-chunk-retries", "2"}}}},
        {});
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensUploadChunkerTest, UploadsSuccessfullyInChunks) {
  FakeUploadChunkerDelegate delegate;
  LensUploadChunker chunker(&delegate,
                            task_environment_.GetMainThreadTaskRunner());

  lens::LensOverlayRequestId request_id;
  request_id.set_sequence_id(1);

  // 7 bytes. With chunk-size-bytes = 3, this should result in 3 chunks: [3, 3,
  // 1] bytes.
  std::vector<uint8_t> page_bytes = {1, 2, 3, 4, 5, 6, 7};
  chunker.Start(request_id, lens::MimeType::kPdf, GURL("https://example.com"),
                "Example Title", page_bytes);

  // Let the background compression task run.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate.uploaded_requests_.size() >= 3u; }));

  // Verify that all 3 chunk uploads were dispatched.
  ASSERT_EQ(3u, delegate.uploaded_requests_.size());
  EXPECT_EQ(0, delegate.uploaded_requests_[0].chunk_id());
  EXPECT_EQ(1, delegate.uploaded_requests_[1].chunk_id());
  EXPECT_EQ(2, delegate.uploaded_requests_[2].chunk_id());

  // Verify request contexts are correct.
  EXPECT_EQ(3, delegate.uploaded_requests_[0].debug_options().total_chunks());
  EXPECT_EQ(1, delegate.uploaded_requests_[0]
                   .request_context()
                   .request_id()
                   .sequence_id());
  EXPECT_EQ(lens::PLATFORM_WEB, delegate.uploaded_requests_[0]
                                    .request_context()
                                    .client_context()
                                    .platform());

  // Simulate progress on first chunk.
  delegate.SimulateChunkProgress(0, 5, 10);
  EXPECT_TRUE(delegate.progress_called_);
  // Cumulative progress might vary slightly due to compression output lengths.
  // Verify that progress is updated.
  EXPECT_GT(delegate.last_position_, 0u);
  EXPECT_GT(delegate.last_total_, 0u);

  // Simulate success completion for the 3 chunks.
  delegate.SimulateChunkResponse(0, google_apis::ApiErrorCode::HTTP_SUCCESS,
                                 "");
  delegate.SimulateChunkResponse(1, google_apis::ApiErrorCode::HTTP_SUCCESS,
                                 "");
  delegate.SimulateChunkResponse(2, google_apis::ApiErrorCode::HTTP_SUCCESS,
                                 "");

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return delegate.payload_ready_called_; }));

  // Verify that the final page content payload is ready.
  EXPECT_TRUE(delegate.payload_ready_called_);
  EXPECT_EQ(3, delegate.last_payload_.content()
                   .content_data(0)
                   .stored_chunk_options()
                   .total_stored_chunks());
  EXPECT_TRUE(delegate.last_payload_.content()
                  .content_data(0)
                  .stored_chunk_options()
                  .read_stored_chunks());
  EXPECT_FALSE(delegate.last_payload_.content()
                   .content_data(0)
                   .stored_chunk_options()
                   .is_read_retry());
  EXPECT_EQ("https://example.com/",
            delegate.last_payload_.content().webpage_url());
  EXPECT_EQ("Example Title", delegate.last_payload_.content().webpage_title());
}

TEST_F(LensUploadChunkerTest, HandlesNetworkErrorOnChunkUpload) {
  FakeUploadChunkerDelegate delegate;
  LensUploadChunker chunker(&delegate,
                            task_environment_.GetMainThreadTaskRunner());

  lens::LensOverlayRequestId request_id;
  request_id.set_sequence_id(1);

  std::vector<uint8_t> page_bytes = {1, 2, 3, 4, 5, 6, 7};
  chunker.Start(request_id, lens::MimeType::kPdf, GURL("https://example.com"),
                "Example Title", page_bytes);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate.uploaded_requests_.size() >= 3u; }));

  // Simulate a network failure on the first chunk.
  delegate.SimulateChunkResponse(
      0, google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, "error");

  // Verify that delegate error is dispatched.
  EXPECT_TRUE(delegate.error_called_);
  EXPECT_EQ(LensUploadChunker::ErrorType::kNetworkError,
            delegate.last_error_type_);
}

TEST_F(LensUploadChunkerTest, InterceptsMissingChunksErrorAndRetries) {
  FakeUploadChunkerDelegate delegate;
  LensUploadChunker chunker(&delegate,
                            task_environment_.GetMainThreadTaskRunner());

  lens::LensOverlayRequestId request_id;
  request_id.set_sequence_id(1);

  std::vector<uint8_t> page_bytes = {1, 2, 3, 4, 5, 6, 7};
  chunker.Start(request_id, lens::MimeType::kPdf, GURL("https://example.com"),
                "Example Title", page_bytes);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return delegate.uploaded_requests_.size() >= 3u; }));

  // Mark all initial chunk uploads as successful.
  for (size_t i = 0; i < 3; ++i) {
    delegate.SimulateChunkResponse(i, google_apis::ApiErrorCode::HTTP_SUCCESS,
                                   "");
  }
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return delegate.payload_ready_called_; }));
  EXPECT_TRUE(delegate.payload_ready_called_);
  delegate.payload_ready_called_ = false;

  // Now mock an objects request server response containing a missing chunks
  // error.
  lens::LensOverlayServerResponse server_response;
  server_response.mutable_error()->set_error_type(
      lens::LensOverlayServerError_ErrorType::
          LensOverlayServerError_ErrorType_MISSING_CHUNKS);
  server_response.mutable_error()
      ->mutable_missing_chunks_metadata()
      ->set_has_chunk_metadata(true);
  // Let's say chunk 1 is missing and needs to be re-uploaded.
  server_response.mutable_error()
      ->mutable_missing_chunks_metadata()
      ->add_missing_chunk_ids(1);

  delegate.uploaded_requests_.clear();

  // Chunker should handle the response and trigger a retry of chunk 1.
  bool handled =
      chunker.HandlePageContentResponse(server_response.SerializeAsString());
  EXPECT_TRUE(handled);

  // Verify that only chunk 1 was requested for upload.
  ASSERT_EQ(1u, delegate.uploaded_requests_.size());
  EXPECT_EQ(1, delegate.uploaded_requests_[0].chunk_id());

  // Complete retry upload successfully.
  delegate.SimulateChunkResponse(1, google_apis::ApiErrorCode::HTTP_SUCCESS,
                                 "");
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return delegate.payload_ready_called_; }));

  // Chunker should trigger payload ready callback again.
  EXPECT_TRUE(delegate.payload_ready_called_);
  EXPECT_TRUE(delegate.last_payload_.content()
                  .content_data(0)
                  .stored_chunk_options()
                  .is_read_retry());
}

}  // namespace

}  // namespace lens

#endif  // !BUILDFLAG(IS_ANDROID)
