// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::babelorca {
namespace {

using RequestDataPtr = std::unique_ptr<RequestDataWrapper>;

constexpr char kOAuthToken[] = "oauth-token";
constexpr char kUrl[] = "https://test.com";

class FakeTachonParsingService : public mojom::TachyonParsingService {
 public:
  explicit FakeTachonParsingService(
      mojo::PendingReceiver<mojom::TachyonParsingService> receiver)
      : receiver_(this, std::move(receiver)) {}

  FakeTachonParsingService(const FakeTachonParsingService&) = delete;
  FakeTachonParsingService& operator=(const FakeTachonParsingService) = delete;

  ~FakeTachonParsingService() override = default;

  void RunParseCallback(mojom::ParsingState state,
                        std::vector<mojom::BabelOrcaMessagePtr> messages,
                        mojom::StreamStatusPtr stream_status) {
    WaitForCallback();
    std::move(callback_).Run(state, std::move(messages),
                             std::move(stream_status));
  }

  int parse_calls() { return parse_calls_; }

 private:
  // mojom::TachyonParsingService:
  void Parse(const std::string& stream_data, ParseCallback callback) override {
    ++parse_calls_;
    callback_ = std::move(callback);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitForCallback() {
    if (callback_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  mojo::Receiver<mojom::TachyonParsingService> receiver_;

  ParseCallback callback_;

  std::unique_ptr<base::RunLoop> run_loop_;

  int parse_calls_ = 0;
};

class TachyonStreamingClientTest : public testing::Test {
 protected:
  RequestDataPtr request_data() {
    auto request_data = std::make_unique<RequestDataWrapper>(
        TRAFFIC_ANNOTATION_FOR_TESTS, kUrl, /*max_retries_param=*/1,
        result_future_.GetCallback());
    request_data->content_data = "request-body";
    return request_data;
  }

  mojom::BabelOrcaMessagePtr babel_orca_message_mojom() {
    mojom::BabelOrcaMessagePtr message = mojom::BabelOrcaMessage::New();
    message->session_id = "session id";
    message->init_timestamp_ms = 1234566789;
    message->order = 3;
    message->current_transcript = mojom::TranscriptPart::New();
    message->current_transcript->transcript_id = 12;
    message->current_transcript->text_index = 5;
    message->current_transcript->text = "some text";
    message->current_transcript->is_final = false;
    message->current_transcript->language = "en";
    return message;
  }

  void CreateStreamingClient() {
    client_ = std::make_unique<TachyonStreamingClient>(
        url_loader_factory_.GetSafeWeakWrapper(),
        base::BindLambdaForTesting([this]() {
          mojo::Remote<mojom::TachyonParsingService> remote;
          parsing_service_ = std::make_unique<FakeTachonParsingService>(
              remote.BindNewPipeAndPassReceiver());
          return remote;
        }),
        on_message_future_.GetCallback());
  }

  std::unique_ptr<TachyonStreamingClient> client_;
  std::unique_ptr<FakeTachonParsingService> parsing_service_;
  base::test::TestFuture<void> resume_future_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::test::TestFuture<RequestDataPtr> auth_failure_future_;
  base::test::TestFuture<TachyonResponse> result_future_;
  base::test::RepeatingTestFuture<mojom::BabelOrcaMessagePtr>
      on_message_future_;
  base::test::TaskEnvironment task_env_;
};

TEST_F(TachyonStreamingClientTest, SuccessfulRequestNoDataStreamed) {
  url_loader_factory_.AddResponse(kUrl, "");

  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());

  TachyonResponse result = result_future_.Take();
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(on_message_future_.IsEmpty());
  EXPECT_FALSE(auth_failure_future_.IsReady());
}

TEST_F(TachyonStreamingClientTest, HttpErrorNoDataStreamed) {
  url_loader_factory_.AddResponse(
      kUrl, "error", net::HttpStatusCode::HTTP_PRECONDITION_FAILED);

  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());

  TachyonResponse result = result_future_.Take();
  EXPECT_EQ(result.status(), TachyonResponse::Status::kHttpError);
  EXPECT_TRUE(on_message_future_.IsEmpty());
  EXPECT_FALSE(auth_failure_future_.IsReady());
}

TEST_F(TachyonStreamingClientTest, AuthErrorNoDataStreamed) {
  url_loader_factory_.AddResponse(kUrl, "error",
                                  net::HttpStatusCode::HTTP_UNAUTHORIZED);

  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());

  RequestDataPtr auth_request_data = auth_failure_future_.Take();
  EXPECT_FALSE(auth_request_data->response_cb.is_null());
  EXPECT_TRUE(on_message_future_.IsEmpty());
}

TEST_F(TachyonStreamingClientTest, DataStreamedSuccess) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  std::vector<mojom::BabelOrcaMessagePtr> messages;
  messages.emplace_back(babel_orca_message_mojom());
  messages.emplace_back(babel_orca_message_mojom());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk,
                                     std::move(messages), nullptr);

  EXPECT_TRUE(resume_future_.Wait());
  EXPECT_FALSE(on_message_future_.IsEmpty());
  on_message_future_.Take();
  EXPECT_FALSE(on_message_future_.IsEmpty());
  on_message_future_.Take();
  EXPECT_TRUE(on_message_future_.IsEmpty());
  EXPECT_FALSE(result_future_.IsReady());
  EXPECT_FALSE(auth_failure_future_.IsReady());
}

TEST_F(TachyonStreamingClientTest, DataStreamedSuccessClosed) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  std::vector<mojom::BabelOrcaMessagePtr> messages;
  messages.emplace_back(babel_orca_message_mojom());
  mojom::StreamStatusPtr stream_status =
      mojom::StreamStatus::New(/*code=*/0, /*message=*/"success");
  parsing_service_->RunParseCallback(mojom::ParsingState::kClosed,
                                     std::move(messages),
                                     std::move(stream_status));

  TachyonResponse result = result_future_.Take();
  EXPECT_TRUE(result.ok());
  EXPECT_FALSE(auth_failure_future_.IsReady());
  EXPECT_FALSE(resume_future_.IsReady());
  EXPECT_FALSE(on_message_future_.IsEmpty());
  on_message_future_.Take();
  EXPECT_TRUE(on_message_future_.IsEmpty());
}

TEST_F(TachyonStreamingClientTest, DataStreamedParsingError) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  std::vector<mojom::BabelOrcaMessagePtr> messages;
  messages.emplace_back(babel_orca_message_mojom());
  mojom::StreamStatusPtr stream_status =
      mojom::StreamStatus::New(/*code=*/0, /*message=*/"success");
  parsing_service_->RunParseCallback(mojom::ParsingState::kError,
                                     std::move(messages),
                                     std::move(stream_status));

  TachyonResponse result = result_future_.Take();
  EXPECT_EQ(result.status(), TachyonResponse::Status::kInternalError);
  EXPECT_FALSE(auth_failure_future_.IsReady());
  EXPECT_FALSE(resume_future_.IsReady());
  EXPECT_FALSE(on_message_future_.IsEmpty());
  on_message_future_.Take();
  EXPECT_TRUE(on_message_future_.IsEmpty());
}

TEST_F(TachyonStreamingClientTest, DataStreamedAuthErrorClosed) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  mojom::StreamStatusPtr stream_status =
      mojom::StreamStatus::New(/*code=*/16, /*message=*/"auth error");
  parsing_service_->RunParseCallback(mojom::ParsingState::kClosed, {},
                                     std::move(stream_status));

  EXPECT_TRUE(auth_failure_future_.Wait());
  EXPECT_FALSE(result_future_.IsReady());
  EXPECT_FALSE(resume_future_.IsReady());
  EXPECT_TRUE(on_message_future_.IsEmpty());
}

TEST_F(TachyonStreamingClientTest, ParsingServiceDisconnected) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk, {}, nullptr);
  EXPECT_TRUE(resume_future_.Wait());
  // Destroy parsing service.
  parsing_service_.reset();

  TachyonResponse result = result_future_.Take();
  EXPECT_EQ(result.status(), TachyonResponse::Status::kInternalError);
}

TEST_F(TachyonStreamingClientTest, UseSingleParsingServicePerConnection) {
  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk, {}, nullptr);
  EXPECT_TRUE(resume_future_.Wait());

  resume_future_.Clear();
  client_->OnDataReceived("456", resume_future_.GetCallback());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk, {}, nullptr);

  EXPECT_TRUE(resume_future_.Wait());
  EXPECT_EQ(parsing_service_->parse_calls(), 2);
}

TEST_F(TachyonStreamingClientTest, ResetParsingServiceOnRetry) {
  base::test::TestFuture<void> retry_future;

  CreateStreamingClient();
  client_->StartRequest(request_data(), kOAuthToken,
                        auth_failure_future_.GetCallback());
  client_->OnDataReceived("123", resume_future_.GetCallback());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk, {}, nullptr);
  EXPECT_TRUE(resume_future_.Wait());
  EXPECT_EQ(parsing_service_->parse_calls(), 1);

  resume_future_.Clear();
  client_->OnRetry(retry_future.GetCallback());
  client_->OnDataReceived("456", resume_future_.GetCallback());
  parsing_service_->RunParseCallback(mojom::ParsingState::kOk, {}, nullptr);

  EXPECT_TRUE(retry_future.IsReady());
  EXPECT_TRUE(resume_future_.Wait());
  EXPECT_EQ(parsing_service_->parse_calls(), 1);
}

}  // namespace
}  // namespace ash::babelorca
