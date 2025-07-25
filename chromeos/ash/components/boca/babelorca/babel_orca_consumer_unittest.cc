// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_consumer.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_streaming_client.h"
#include "chromeos/ash/components/boca/babelorca/testing_utils.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::babelorca {
namespace {

const std::string kEnglishUsLocale = "en-US";
const std::string kSpanishUsLocale = "es-US";
const GaiaId::Literal kGaiaId("gaia-id");
const std::string kSessionId = "session_id";
const std::string kEmail = "test@school.edu";
constexpr char kTestSTUrl[] = "https://test";
constexpr char kReceivingStoppedReasonUma[] =
    "Ash.Boca.Babelorca.ReceivingStoppedReason";

class BabelOrcaConsumerTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterPrefsForTesting(&pref_service_);
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSync);
  }

  void TearDown() override { caption_controller_delegate_ = nullptr; }

  void CreateConsumer(bool translate_enabled = false) {
    auto caption_controller_delegate =
        std::make_unique<FakeCaptionControllerDelegate>();
    caption_controller_delegate_ = caption_controller_delegate.get();
    auto caption_bubble_settings = std::make_unique<CaptionBubbleSettingsImpl>(
        &pref_service_, kEnglishUsLocale, base::DoNothing());
    caption_bubble_settings_ = caption_bubble_settings.get();
    auto caption_controller = std::make_unique<CaptionController>(
        /*caption_bubble_context=*/
        nullptr, &pref_service_, kEnglishUsLocale,
        std::move(caption_bubble_settings),
        std::move(caption_controller_delegate));
    caption_controller->SetLiveTranslateEnabled(translate_enabled);
    auto fake_translation_dispatcher =
        std::make_unique<FakeBabelOrcaTranslationDispatcher>();
    fake_translation_dispatcher_ = fake_translation_dispatcher->GetWeakPtr();

    consumer_ = std::make_unique<BabelOrcaConsumer>(
        url_loader_factory_.GetSafeWeakWrapper(),
        identity_test_env_.identity_manager(), kGaiaId, kTestSTUrl,
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

  std::string JoinGroupUrl() {
    return base::StrCat({kTestSTUrl, base::ReplaceStringPlaceholders(
                                         boca::kJoinTachyonGroupUrlTemplate,
                                         {kGaiaId.ToString(), kSessionId},
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
    message->current_transcript->language = kEnglishUsLocale;
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
  raw_ptr<CaptionBubbleSettingsImpl> caption_bubble_settings_;
  raw_ptr<FakeCaptionControllerDelegate> caption_controller_delegate_;
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher>
      fake_translation_dispatcher_;
  base::HistogramTester uma_recorder_;
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnSessionEnded();
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
  EXPECT_EQ(uma_recorder_.GetBucketCount(
                kReceivingStoppedReasonUma,
                BabelOrcaConsumer::ReceivingStoppedReason::kSessionEnded),
            1);
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false,
                                           /*translations_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
  EXPECT_EQ(
      uma_recorder_.GetBucketCount(
          kReceivingStoppedReasonUma,
          BabelOrcaConsumer::ReceivingStoppedReason::kSessionCaptionTurnedOff),
      1);
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
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());

  ASSERT_TRUE(streaming_client_waiter_.Wait());
  EXPECT_TRUE(caption_controller_delegate_->IsCaptionBubbleAlive());

  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_->IsCaptionBubbleAlive());
  EXPECT_EQ(
      uma_recorder_.GetBucketCount(
          kReceivingStoppedReasonUma,
          BabelOrcaConsumer::ReceivingStoppedReason::kLocalCaptionTurnedOff),
      1);
}

TEST_F(BabelOrcaConsumerTest,
       EnableTranslationsTranslationToggleFeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kBocaTranslateToggle);
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  EXPECT_TRUE(caption_bubble_settings_->IsLiveTranslateFeatureEnabled());
  EXPECT_TRUE(caption_bubble_settings_->GetLiveTranslateEnabled());
}

TEST_F(BabelOrcaConsumerTest,
       AllowAndDisableTranslationsTranslationToggleFeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBocaTranslateToggle);
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  EXPECT_TRUE(caption_bubble_settings_->IsLiveTranslateFeatureEnabled());
  EXPECT_FALSE(caption_bubble_settings_->GetLiveTranslateEnabled());
}

TEST_F(BabelOrcaConsumerTest,
       AllowAndEnableTranslationsTranslationToggleFeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBocaTranslateToggle);
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer(/*translate_enabled=*/true);
  consumer_->OnSessionStarted();
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/true);
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
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
  EXPECT_TRUE(caption_bubble_settings_->IsLiveTranslateFeatureEnabled());
  EXPECT_TRUE(caption_bubble_settings_->GetLiveTranslateEnabled());
}

TEST_F(BabelOrcaConsumerTest, OnLanguageIdentificationEvent) {
  request_data_provider_ = std::make_unique<FakeTachyonRequestDataProvider>(
      kSessionId, "tachyon-token", "group_id", kEmail);
  CreateConsumer();
  consumer_->OnSessionStarted();
  consumer_->OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  consumer_->OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                           /*translations_enabled=*/false);

  // Join Tachyon group.
  url_loader_factory_.AddResponse(JoinGroupUrl(), "");
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "oauth_token", base::Time::Max());
  ASSERT_TRUE(streaming_client_waiter_.Wait());

  ASSERT_FALSE(on_message_cb_.is_null());

  // Language identification event will initially be triggered.
  mojom::BabelOrcaMessagePtr message_1 = CreateMessage();
  on_message_cb_.Run(std::move(message_1));
  ASSERT_THAT(caption_controller_delegate_->GetLanguageIdentificationEvents(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_->GetLanguageIdentificationEvents()
                .at(0)
                ->language,
            kEnglishUsLocale);

  // Language not changed so no language identification event.
  mojom::BabelOrcaMessagePtr message_2 = CreateMessage();
  message_2->order = 2;
  on_message_cb_.Run(std::move(message_2));
  EXPECT_THAT(caption_controller_delegate_->GetLanguageIdentificationEvents(),
              testing::SizeIs(1));

  // Language identification event will be triggered when switching language.
  mojom::BabelOrcaMessagePtr message_3 = CreateMessage();
  message_3->order = 3;
  message_3->current_transcript->language = kSpanishUsLocale;
  on_message_cb_.Run(std::move(message_3));
  ASSERT_THAT(caption_controller_delegate_->GetLanguageIdentificationEvents(),
              testing::SizeIs(2));
  EXPECT_EQ(caption_controller_delegate_->GetLanguageIdentificationEvents()
                .at(1)
                ->language,
            kSpanishUsLocale);

  mojom::BabelOrcaMessagePtr message_4 = CreateMessage();
  message_4->order = 4;
  message_4->current_transcript->language = kEnglishUsLocale;
  on_message_cb_.Run(std::move(message_4));
  ASSERT_THAT(caption_controller_delegate_->GetLanguageIdentificationEvents(),
              testing::SizeIs(3));
  EXPECT_EQ(caption_controller_delegate_->GetLanguageIdentificationEvents()
                .at(2)
                ->language,
            kEnglishUsLocale);
}

}  // namespace
}  // namespace ash::babelorca
