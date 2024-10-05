// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_receiver.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

using ResultFuture =
    base::test::RepeatingTestFuture<media::SpeechRecognitionResult,
                                    std::string>;

const std::string kLanguage = "fr";
constexpr base::TimeDelta kInitialBackoff = base::Milliseconds(250);

class TranscriptReceiverTest : public testing::Test {
 protected:
  void SetUp() override {
    receiver_ = std::make_unique<TranscriptReceiver>(
        url_loader_factory_.GetSafeWeakWrapper(), TRAFFIC_ANNOTATION_FOR_TESTS,
        &data_provider_,
        base::BindLambdaForTesting(
            [this](
                scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
                TachyonStreamingClient::OnMessageCallback on_message_cb)
                -> std::unique_ptr<TachyonAuthedClient> {
              on_message_callback_ = std::move(on_message_cb);
              auto authed_client = std::make_unique<FakeTachyonAuthedClient>();
              authed_client_ptr_ = authed_client.get();
              return authed_client;
            }),
        /*max_retries=*/2);
  }

  mojom::BabelOrcaMessagePtr CreateMessage(const std::string& text,
                                           bool is_final,
                                           const std::string& sender_email,
                                           const std::string& session_id) {
    auto current_transcript = mojom::TranscriptPart::New(
        /*transcript_id=*/1234, /*text_index=*/0, text, is_final, kLanguage);
    ++message_order_;
    return mojom::BabelOrcaMessage::New(
        sender_email, session_id,
        /*init_timestamp_ms=*/999999, message_order_,
        /*previous_transcript_part=*/nullptr, std::move(current_transcript));
  }

  mojom::BabelOrcaMessagePtr CreateMessage(const std::string& text,
                                           bool is_final) {
    return CreateMessage(text, is_final, data_provider_.sender_email(),
                         data_provider_.session_id());
  }

  void VerifyMessage(ResultFuture* result_future,
                     const std::string& text,
                     bool is_final) {
    ASSERT_FALSE(result_future->IsEmpty());
    auto [result, language] = result_future->Take();
    EXPECT_EQ(result.transcription, text);
    EXPECT_EQ(result.is_final, is_final);
    EXPECT_EQ(language, kLanguage);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory url_loader_factory_;
  FakeTachyonRequestDataProvider data_provider_;
  std::unique_ptr<TranscriptReceiver> receiver_;
  raw_ptr<FakeTachyonAuthedClient> authed_client_ptr_;
  TachyonStreamingClient::OnMessageCallback on_message_callback_;
  int message_order_ = 0;
};

TEST_F(TranscriptReceiverTest, MultipleMessages) {
  const std::string kText1 = "first msg";
  const std::string kText2 = "second msg";
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());

  on_message_callback_.Run(CreateMessage(kText1, /*is_final=*/true));
  on_message_callback_.Run(CreateMessage(kText2, /*is_final=*/false));

  VerifyMessage(&result_future, kText1, /*is_final=*/true);
  VerifyMessage(&result_future, kText2, /*is_final=*/false);
  EXPECT_TRUE(result_future.IsEmpty());
  EXPECT_FALSE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, VerifyEmailAndSessionId) {
  const std::string kText1 = "first msg";
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());

  on_message_callback_.Run(CreateMessage("random text", /*is_final=*/false,
                                         data_provider_.sender_email(),
                                         "wrong session id"));
  on_message_callback_.Run(CreateMessage("other random text",
                                         /*is_final=*/false, "wrong@email.com",
                                         data_provider_.session_id()));
  on_message_callback_.Run(CreateMessage(kText1, /*is_final=*/true));

  VerifyMessage(&result_future, kText1, /*is_final=*/true);
  EXPECT_TRUE(result_future.IsEmpty());
  EXPECT_FALSE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, RestartsStreamingWhenEndedByServer) {
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kOk));

  authed_client_ptr_->WaitForRequest();
  EXPECT_TRUE(result_future.IsEmpty());
  EXPECT_FALSE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, NoRetryOnAuthError) {
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kAuthError));

  EXPECT_TRUE(result_future.IsEmpty());
  EXPECT_TRUE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, FailAfterMaxRetries) {
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));
  EXPECT_FALSE(failure_future.IsReady());

  // 1st retry
  task_environment_.FastForwardBy(kInitialBackoff);
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));
  EXPECT_FALSE(failure_future.IsReady());

  // 2nd retry
  task_environment_.FastForwardBy(kInitialBackoff * 2);
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kNetworkError));

  EXPECT_TRUE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, ResetRetriesOnMessageReceived) {
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  // 1st retry
  task_environment_.FastForwardBy(kInitialBackoff);
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  // 2nd retry
  task_environment_.FastForwardBy(kInitialBackoff * 2);
  // New message received should reset retries.
  on_message_callback_.Run(CreateMessage("trasncript", /*is_final=*/true));
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  EXPECT_FALSE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, ResetRetriesOnOkStatus) {
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  // 1st retry
  task_environment_.FastForwardBy(kInitialBackoff);
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  // 2nd retry
  task_environment_.FastForwardBy(kInitialBackoff * 2);
  // Ok status should reset retries.
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kOk));
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  EXPECT_FALSE(failure_future.IsReady());
}

TEST_F(TranscriptReceiverTest, StartReceivingShouldResetRetries) {
  ResultFuture result_future1;
  base::test::TestFuture<void> failure_future1;
  ResultFuture result_future2;
  base::test::TestFuture<void> failure_future2;

  receiver_->StartReceiving(result_future1.GetCallback(),
                            failure_future1.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  // 1st retry
  task_environment_.FastForwardBy(kInitialBackoff);
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  // 2nd retry
  task_environment_.FastForwardBy(kInitialBackoff * 2);
  // New request before the retry response.
  receiver_->StartReceiving(result_future2.GetCallback(),
                            failure_future2.GetCallback());
  authed_client_ptr_->ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kInternalError));

  EXPECT_FALSE(failure_future1.IsReady());
  EXPECT_FALSE(failure_future2.IsReady());
}

TEST_F(TranscriptReceiverTest, OneMessageWithMultipleTranscripts) {
  const std::string kText1 = "first msg";
  const std::string kText2 = "second msg";
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());

  auto transcript = mojom::TranscriptPart::New(
      /*transcript_id=*/1234, /*text_index=*/0, kText1, /*is_final=*/false,
      kLanguage);
  on_message_callback_.Run(mojom::BabelOrcaMessage::New(
      data_provider_.sender_email(), data_provider_.session_id(),
      /*init_timestamp_ms=*/999999, /*order=*/1,
      /*previous_transcript_part=*/nullptr,
      /*current_transcript_part=*/transcript->Clone()));

  // Set previous transcript to test multiple transcripts on one streaming
  // chunk.
  transcript->is_final = true;
  on_message_callback_.Run(mojom::BabelOrcaMessage::New(
      data_provider_.sender_email(), data_provider_.session_id(),
      /*init_timestamp_ms=*/999999, /*order=*/2,
      /*previous_transcript_part=*/std::move(transcript),
      /*current_transcript_part=*/
      mojom::TranscriptPart::New(
          /*transcript_id=*/5678, /*text_index=*/0, kText2, /*is_final=*/false,
          kLanguage)));

  VerifyMessage(&result_future, kText1, /*is_final=*/false);
  VerifyMessage(&result_future, kText1, /*is_final=*/true);
  VerifyMessage(&result_future, kText2, /*is_final=*/false);
  EXPECT_TRUE(result_future.IsEmpty());
}

TEST_F(TranscriptReceiverTest, NewRequestResetsTranscriptBuilder) {
  const std::string kText1 = "first msg";
  const std::string kText2 = "second msg";
  ResultFuture result_future;
  base::test::TestFuture<void> failure_future;

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());

  auto transcript = mojom::TranscriptPart::New(
      /*transcript_id=*/1234, /*text_index=*/0, kText1, /*is_final=*/false,
      kLanguage);
  on_message_callback_.Run(mojom::BabelOrcaMessage::New(
      data_provider_.sender_email(), data_provider_.session_id(),
      /*init_timestamp_ms=*/999999, /*order=*/1,
      /*previous_transcript_part=*/nullptr,
      /*current_transcript_part=*/transcript->Clone()));

  receiver_->StartReceiving(result_future.GetCallback(),
                            failure_future.GetCallback());
  transcript->is_final = true;
  on_message_callback_.Run(mojom::BabelOrcaMessage::New(
      data_provider_.sender_email(), data_provider_.session_id(),
      /*init_timestamp_ms=*/999999, /*order=*/2,
      /*previous_transcript_part=*/std::move(transcript),
      /*current_transcript_part=*/
      mojom::TranscriptPart::New(
          /*transcript_id=*/5678, /*text_index=*/0, kText2, /*is_final=*/false,
          kLanguage)));

  VerifyMessage(&result_future, kText1, /*is_final=*/false);
  VerifyMessage(&result_future, kText2, /*is_final=*/false);
  EXPECT_TRUE(result_future.IsEmpty());
}

}  // namespace
}  // namespace ash::babelorca
