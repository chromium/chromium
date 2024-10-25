// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_consumer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"
#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-forward.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::babelorca {
namespace {

const std::string kGaiaId = "gaia-id";
const std::string kSessionId = "session_id";
const std::string kEmail = "test@school.edu";

class MockLiveCaptionControllerWrapper : public LiveCaptionControllerWrapper {
 public:
  MockLiveCaptionControllerWrapper() = default;
  ~MockLiveCaptionControllerWrapper() override = default;
  MOCK_METHOD(bool,
              DispatchTranscription,
              (const media::SpeechRecognitionResult&),
              (override));
  MOCK_METHOD(void, ToggleLiveCaptionForBabelOrca, (bool), (override));
  MOCK_METHOD(void, OnAudioStreamEnd, (), (override));
};

class BabelOrcaConsumerTest : public testing::Test {
 protected:
  void SetUp() override {
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSync);
  }

  void TearDown() override {
    EXPECT_CALL(*caption_controller_wrapper_, OnAudioStreamEnd).Times(1);
    caption_controller_wrapper_ = nullptr;
    consumer_.reset();
  }

  void CreateConsumer() {
    auto caption_controller_wrapper =
        std::make_unique<testing::NiceMock<MockLiveCaptionControllerWrapper>>();
    caption_controller_wrapper_ = caption_controller_wrapper.get();
    live_caption_enabled_ = false;
    ON_CALL(*caption_controller_wrapper_, ToggleLiveCaptionForBabelOrca)
        .WillByDefault(
            [this](bool enabled) { live_caption_enabled_ = enabled; });

    consumer_ = std::make_unique<BabelOrcaConsumer>(
        url_loader_factory_.GetSafeWeakWrapper(),
        identity_test_env_.identity_manager(), kGaiaId,
        std::move(caption_controller_wrapper), &token_manager_,
        request_data_provider_.get(),
        base::BindLambdaForTesting(
            [this](
                scoped_refptr<network::SharedURLLoaderFactory>,
                TachyonStreamingClient::OnMessageCallback on_message_callback)
                -> std::unique_ptr<TachyonAuthedClient> {
              on_message_cb_ = std::move(on_message_callback);
              streaming_client_waiter_.GetCallback().Run();
              return std::make_unique<FakeTachyonAuthedClient>();
            }));
  }

  std::string url() {
    return base::StrCat(
        {boca::kSchoolToolsApiBaseUrl,
         base::ReplaceStringPlaceholders(boca::kJoinTachyonGroupUrlTemplate,
                                         {kGaiaId, kSessionId},
                                         /*=offsets*/ nullptr)});
  }

  mojom::BabelOrcaMessagePtr CreateMessage() {
    mojom::BabelOrcaMessagePtr message = mojom::BabelOrcaMessage::New();
    message->session_id = kSessionId;
    message->sender_email = kEmail;
    message->init_timestamp_ms = 1234;
    message->order = 1;
    message->current_transcript = mojom::TranscriptPart::New();
    message->current_transcript->transcript_id = 1;
    message->current_transcript->is_final = true;
    message->current_transcript->text_index = 0;
    message->current_transcript->text = "transcript";
    message->current_transcript->language = "en";
    return message;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<FakeTachyonRequestDataProvider> request_data_provider_;
  AccountInfo account_info_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakeTokenManager token_manager_;
  TachyonStreamingClient::OnMessageCallback on_message_cb_;
  std::unique_ptr<BabelOrcaConsumer> consumer_;
  base::test::RepeatingTestFuture<void> streaming_client_waiter_;
  raw_ptr<MockLiveCaptionControllerWrapper> caption_controller_wrapper_;
  bool live_caption_enabled_;
};

TEST_F(BabelOrcaConsumerTest, SessionThenLocalEnabledNotSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, /*tachyon_token=*/std::nullopt, "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Signin to tachyon.
  base::OnceCallback<void(bool)> signin_cb =
      request_data_provider_->TakeSigninCb();
  request_data_provider_->set_tachyon_token("tachyon-token");
  ASSERT_FALSE(signin_cb.is_null());
  std::move(signin_cb).Run(true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(live_caption_enabled_);
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  EXPECT_CALL(*caption_controller_wrapper_,
              DispatchTranscription(media::SpeechRecognitionResult(
                  message->current_transcript->text,
                  message->current_transcript->is_final)))
      .Times(1);
  on_message_cb_.Run(std::move(message));
}

TEST_F(BabelOrcaConsumerTest, LocalThenSessionEnabledNotSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, /*tachyon_token=*/std::nullopt, "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);

  // Signin to tachyon.
  base::OnceCallback<void(bool)> signin_cb =
      request_data_provider_->TakeSigninCb();
  request_data_provider_->set_tachyon_token("tachyon-token");
  ASSERT_FALSE(signin_cb.is_null());
  std::move(signin_cb).Run(true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(live_caption_enabled_);
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  EXPECT_CALL(*caption_controller_wrapper_,
              DispatchTranscription(media::SpeechRecognitionResult(
                  message->current_transcript->text,
                  message->current_transcript->is_final)))
      .Times(1);
  on_message_cb_.Run(std::move(message));
}

TEST_F(BabelOrcaConsumerTest, SessionThenLocalEnabledSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(live_caption_enabled_);
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  EXPECT_CALL(*caption_controller_wrapper_,
              DispatchTranscription(media::SpeechRecognitionResult(
                  message->current_transcript->text,
                  message->current_transcript->is_final)))
      .Times(1);
  on_message_cb_.Run(std::move(message));
}

TEST_F(BabelOrcaConsumerTest, LocalThenSessionEnabledSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(live_caption_enabled_);
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  EXPECT_CALL(*caption_controller_wrapper_,
              DispatchTranscription(media::SpeechRecognitionResult(
                  message->current_transcript->text,
                  message->current_transcript->is_final)))
      .Times(1);
  on_message_cb_.Run(std::move(message));
}

TEST_F(BabelOrcaConsumerTest, OnSessionEnded) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(live_caption_enabled_);

  EXPECT_CALL(*caption_controller_wrapper_, OnAudioStreamEnd).Times(1);
  consumer_->OnSessionEnded();
  EXPECT_FALSE(live_caption_enabled_);
}

TEST_F(BabelOrcaConsumerTest, DisableSessionCaptions) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(live_caption_enabled_);

  EXPECT_CALL(*caption_controller_wrapper_, OnAudioStreamEnd).Times(1);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false);
  EXPECT_FALSE(live_caption_enabled_);
}

TEST_F(BabelOrcaConsumerTest, DisableLocalCaptions) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(live_caption_enabled_);

  EXPECT_CALL(*caption_controller_wrapper_, OnAudioStreamEnd).Times(1);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(live_caption_enabled_);
}

}  // namespace
}  // namespace ash::babelorca
