// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_consumer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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

const std::string kApplicationLocale = "en-US";
const std::string kGaiaId = "gaia-id";
const std::string kSessionId = "session_id";
const std::string kEmail = "test@school.edu";
const std::string kTranslationTargetLocale = "de-DE";

const std::string kCaptionsTextSize = "20%";
const std::string kCaptionsTextFont = "aerial";
const std::string kCaptionsTextColor = "255,99,71";
const std::string kCaptionsBackgroundColor = "90,255,50";
const std::string kCaptionsTextShadow = "10px";

constexpr int kCaptionsTextOpacity = 50;
constexpr int kCaptionsBackgroundOpacity = 30;

void RegisterPrefs(TestingPrefServiceSimple* pref_service) {
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextSize, kCaptionsTextSize);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextFont, kCaptionsTextFont);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextColor, kCaptionsTextColor);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsTextOpacity, kCaptionsTextOpacity);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsBackgroundColor, kCaptionsBackgroundColor);
  pref_service->registry()->RegisterStringPref(
      prefs::kAccessibilityCaptionsTextShadow, kCaptionsTextShadow);
  pref_service->registry()->RegisterIntegerPref(
      prefs::kAccessibilityCaptionsBackgroundOpacity,
      kCaptionsBackgroundOpacity);
  pref_service->registry()->RegisterStringPref(
      prefs::kUserMicrophoneCaptionLanguageCode, kApplicationLocale);
  pref_service->registry()->RegisterStringPref(
      prefs::kLiveTranslateTargetLanguageCode, kTranslationTargetLocale);
}

class BabelOrcaConsumerTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterPrefs(&pref_service_);
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSync);
  }

  void TearDown() override { caption_controller_delegate_ = nullptr; }

  void CreateConsumer() {
    auto caption_controller_delegate =
        std::make_unique<FakeCaptionControllerDelegate>();
    caption_controller_delegate_ = caption_controller_delegate.get();
    auto caption_controller = std::make_unique<CaptionController>(
        /*caption_bubble_context=*/nullptr, &pref_service_, kApplicationLocale,
        std::move(caption_controller_delegate));
    auto fake_translation_dispatcher =
        std::make_unique<FakeBabelOrcaTranslationDispatcher>();
    fake_translation_dispatcher_ = fake_translation_dispatcher->GetWeakPtr();

    consumer_ = std::make_unique<BabelOrcaConsumer>(
        url_loader_factory_.GetSafeWeakWrapper(),
        identity_test_env_.identity_manager(), kGaiaId,
        std::move(caption_controller), &token_manager_,
        request_data_provider_.get(),
        base::BindLambdaForTesting(
            [this](
                scoped_refptr<network::SharedURLLoaderFactory>,
                TachyonStreamingClient::OnMessageCallback on_message_callback)
                -> std::unique_ptr<TachyonAuthedClient> {
              on_message_cb_ = std::move(on_message_callback);
              streaming_client_waiter_.GetCallback().Run();
              return std::make_unique<FakeTachyonAuthedClient>();
            }),
        std::make_unique<BabelOrcaCaptionTranslator>(
            std::move(fake_translation_dispatcher)),
        &pref_service_);
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
  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<FakeTachyonRequestDataProvider> request_data_provider_;
  AccountInfo account_info_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakeTokenManager token_manager_;
  TachyonStreamingClient::OnMessageCallback on_message_cb_;
  std::unique_ptr<BabelOrcaConsumer> consumer_;
  base::test::RepeatingTestFuture<void> streaming_client_waiter_;
  raw_ptr<FakeCaptionControllerDelegate> caption_controller_delegate_;
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher>
      fake_translation_dispatcher_;
};

TEST_F(BabelOrcaConsumerTest, SessionThenLocalEnabledNotSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, /*tachyon_token=*/std::nullopt, "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           false);
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

  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  media::SpeechRecognitionResult transcript(
      message->current_transcript->text, message->current_transcript->is_final);
  on_message_cb_.Run(std::move(message));

  ASSERT_THAT(caption_controller_delegate_->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetTranscriptions().at(0),
            transcript);
}

TEST_F(BabelOrcaConsumerTest, LocalThenSessionEnabledNotSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, /*tachyon_token=*/std::nullopt, "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

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

  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  media::SpeechRecognitionResult transcript(
      message->current_transcript->text, message->current_transcript->is_final);
  on_message_cb_.Run(std::move(message));

  ASSERT_THAT(caption_controller_delegate_->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetTranscriptions().at(0),
            transcript);
}

TEST_F(BabelOrcaConsumerTest, SessionThenLocalEnabledSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  media::SpeechRecognitionResult transcript(
      message->current_transcript->text, message->current_transcript->is_final);
  on_message_cb_.Run(std::move(message));

  ASSERT_THAT(caption_controller_delegate_->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetTranscriptions().at(0),
            transcript);
}

TEST_F(BabelOrcaConsumerTest, LocalThenSessionEnabledSignedIn) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  media::SpeechRecognitionResult transcript(
      message->current_transcript->text, message->current_transcript->is_final);
  on_message_cb_.Run(std::move(message));

  ASSERT_THAT(caption_controller_delegate_->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetTranscriptions().at(0),
            transcript);
  EXPECT_EQ(fake_translation_dispatcher_->GetNumGetTranslationCalls(), 0);
}

TEST_F(BabelOrcaConsumerTest, OnSessionEnded) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnSessionEnded();
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
}

TEST_F(BabelOrcaConsumerTest, DisableSessionCaptions) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false,
                                           /*translations_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
}

TEST_F(BabelOrcaConsumerTest, DisableLocalCaptions) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
}

TEST_F(BabelOrcaConsumerTest, EnableTranslations) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(url(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());
  ASSERT_FALSE(on_message_cb_.is_null());
  mojom::BabelOrcaMessagePtr message = CreateMessage();
  media::SpeechRecognitionResult transcript(
      message->current_transcript->text, message->current_transcript->is_final);
  on_message_cb_.Run(std::move(message));

  ASSERT_THAT(caption_controller_delegate_->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetTranscriptions().at(0),
            transcript);
  EXPECT_EQ(fake_translation_dispatcher_->GetNumGetTranslationCalls(), 1);
}

}  // namespace
}  // namespace ash::babelorca
