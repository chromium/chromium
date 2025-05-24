// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_producer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "chromeos/ash/components/boca/babelorca/caption_bubble_settings_impl.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_token_manager.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_translation_dispatcher.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/testing_utils.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

const std::string kLanguage = "en-US";
constexpr char kSendingStoppedReasonUma[] =
    "Ash.Boca.Babelorca.SendingStoppedReason";

class MockSpeechRecognizer : public BabelOrcaSpeechRecognizer {
 public:
  MockSpeechRecognizer() = default;
  ~MockSpeechRecognizer() override = default;
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer * obs), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * obs), (override));
};

class BabelOrcaProducerTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterPrefsForTesting(&pref_service_);
    speech_recognizer_ =
        std::make_unique<testing::NiceMock<MockSpeechRecognizer>>();
    caption_controller_delegate_ =
        std::make_unique<FakeCaptionControllerDelegate>();
    authed_client_ = std::make_unique<FakeTachyonAuthedClient>();

    auto fake_translation_dispatcher =
        std::make_unique<FakeBabelOrcaTranslationDispatcher>();
    translation_dispatcher_ = fake_translation_dispatcher->GetWeakPtr();

    translator_ = std::make_unique<BabelOrcaCaptionTranslator>(
        std::move(fake_translation_dispatcher));
  }

  media::SpeechRecognitionResult GetTranscriptFromRequest(
      const std::string& request) {
    InboxSendRequest send_request;
    CHECK(send_request.ParseFromString(request));
    BabelOrcaMessage message;
    CHECK(message.ParseFromString(send_request.message().message()));
    return media::SpeechRecognitionResult(
        message.current_transcript().text(),
        message.current_transcript().is_final());
  }

  std::unique_ptr<CaptionController> GetCaptionController() {
    auto caption_bubble_settings = std::make_unique<CaptionBubbleSettingsImpl>(
        &pref_service_, kLanguage, base::DoNothing());
    return std::make_unique<CaptionController>(
        /*caption_bubble_context=*/
        nullptr, &pref_service_, kLanguage, std::move(caption_bubble_settings),
        std::move(caption_controller_delegate_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<MockSpeechRecognizer> speech_recognizer_;
  std::unique_ptr<FakeCaptionControllerDelegate> caption_controller_delegate_;
  std::unique_ptr<FakeTachyonAuthedClient> authed_client_;
  FakeTachyonRequestDataProvider request_data_provider_;
  base::WeakPtr<FakeBabelOrcaTranslationDispatcher> translation_dispatcher_;
  std::unique_ptr<BabelOrcaCaptionTranslator> translator_;
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester uma_recorder_;
};

TEST_F(BabelOrcaProducerTest, EnableLocalCaptionsOutOfSession) {
  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/true);
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &request_data_provider_, std::move(translator_));
  EXPECT_CALL(*speech_recognizer_ptr, AddObserver).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  EXPECT_TRUE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());

  producer.OnTranscriptionResult(transcript, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            transcript);

  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(1);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());

  // Stop recognition methods are called on`producer` destruction as a safe
  // guard in case the object was destroyed before stopping recognition.
  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(1);
}

TEST_F(BabelOrcaProducerTest, EnableSessionCaptionsOutOfSession) {
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &request_data_provider_, std::move(translator_));

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver).Times(0);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(0);
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);
}

TEST_F(BabelOrcaProducerTest, EnableSessionCaptionsThenLocalCaptionsInSession) {
  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/true);
  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  FakeTachyonAuthedClient* authed_client_ptr = authed_client_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  EXPECT_CALL(*speech_recognizer_ptr, AddObserver).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  data_provider.set_tachyon_token("tachyon_token");
  std::move(signin_cb).Run(true);

  producer.OnTranscriptionResult(transcript1, kLanguage);
  authed_client_ptr->WaitForRequest();
  media::SpeechRecognitionResult sent_transcript1 =
      GetTranscriptFromRequest(authed_client_ptr->GetRequestString());
  EXPECT_EQ(sent_transcript1, transcript1);
  // Local captions not enabled.
  EXPECT_TRUE(caption_controller_delegate_ptr->GetTranscriptions().empty());

  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  producer.OnTranscriptionResult(transcript2, kLanguage);
  authed_client_ptr->WaitForRequest();
  media::SpeechRecognitionResult sent_transcript2 =
      GetTranscriptFromRequest(authed_client_ptr->GetRequestString());
  EXPECT_EQ(sent_transcript2, transcript2);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            transcript2);

  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  // 2 Times, one on enabled set to false and one on destruction.
  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(2);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(2);
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false,
                                         /*translations_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());
  EXPECT_EQ(
      uma_recorder_.GetBucketCount(
          kSendingStoppedReasonUma,
          BabelOrcaProducer::SendingStoppedReason::kSessionCaptionTurnedOff),
      1);
}

TEST_F(BabelOrcaProducerTest, EnableLocalCaptionsThenSessionCaptionsInSession) {
  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/true);
  media::SpeechRecognitionResult transcript2("transcript2", /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  FakeTachyonAuthedClient* authed_client_ptr = authed_client_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  producer.OnTranscriptionResult(transcript1, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            transcript1);

  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  data_provider.set_tachyon_token("tachyon_token");
  std::move(signin_cb).Run(true);
  producer.OnTranscriptionResult(transcript2, kLanguage);
  authed_client_ptr->WaitForRequest();
  media::SpeechRecognitionResult sent_transcript2 =
      GetTranscriptFromRequest(authed_client_ptr->GetRequestString());
  EXPECT_EQ(sent_transcript2, transcript2);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(2));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[1],
            transcript2);

  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false,
                                         /*translations_enabled=*/false);

  // 2 Times, one on enabled set to false and one on destruction.
  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(2);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(2);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());
}

TEST_F(BabelOrcaProducerTest, NoSigninIfTachyonTokenIsSet) {
  FakeTachyonRequestDataProvider data_provider("session-id", "tachyon_token",
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_TRUE(signin_cb.is_null());
}

TEST_F(BabelOrcaProducerTest, FailedSignWillNotStartCaptions) {
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(0);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(0);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  std::move(signin_cb).Run(false);
}

TEST_F(BabelOrcaProducerTest, DisableSessionCaptionWhileSigninInFlight) {
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/false,
                                         /*translations_enabled=*/false);
  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(0);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(0);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  data_provider.set_tachyon_token("tachyon_token");
  std::move(signin_cb).Run(true);
}

TEST_F(BabelOrcaProducerTest, SessionEndedWhileSigninInFlight) {
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  producer.OnSessionEnded();
  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(0);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(0);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  data_provider.set_tachyon_token("tachyon_token");
  std::move(signin_cb).Run(true);
}

TEST_F(BabelOrcaProducerTest, SessionEndLocalCaptionsDisabled) {
  FakeTachyonRequestDataProvider data_provider("session-id", "tachyon_token",
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  // 2 Times, one on `OnSessionEnded` and one on destruction.
  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(2);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(2);
  producer.OnSessionEnded();
  EXPECT_EQ(uma_recorder_.GetBucketCount(
                kSendingStoppedReasonUma,
                BabelOrcaProducer::SendingStoppedReason::kSessionEnded),
            1);
}

TEST_F(BabelOrcaProducerTest, SessionEndLocalCaptionsEnabled) {
  FakeTachyonRequestDataProvider data_provider("session-id", "tachyon_token",
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(0);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(0);
  producer.OnSessionEnded();
  EXPECT_TRUE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());

  // Stop recognition on destruction.
  EXPECT_CALL(*speech_recognizer_ptr, RemoveObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Stop).Times(1);
}

TEST_F(BabelOrcaProducerTest, DisableLocalWhileSessionCaptionsEnabled) {
  media::SpeechRecognitionResult transcript("transcript", /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id", "tachyon-token",
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);

  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/false);
  EXPECT_FALSE(caption_controller_delegate_ptr->IsCaptionBubbleAlive());
}

TEST_F(BabelOrcaProducerTest, EnableTranslations) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kBocaTranslateToggle);
  media::SpeechRecognitionResult transcript1("transcript1", /*is_final=*/true);
  media::SpeechRecognitionResult transcript2("transcript3", /*is_final=*/true);
  std::string translated_transcript_string = "translated_transcript";
  media::SpeechRecognitionResult translated_transcript(
      translated_transcript_string, /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  translation_dispatcher_->InjectTranslationResult(
      translated_transcript_string);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  producer.OnTranscriptionResult(transcript1, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            translated_transcript);

  // Now we set a target language that is distinct from the source
  // language to ensure that we translate when relevant.
  pref_service_.SetString(prefs::kTranslateTargetLanguageCode,
                          kTranslationTargetLocale);

  producer.OnTranscriptionResult(transcript2, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(2));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[1],
            translated_transcript);
}

TEST_F(BabelOrcaProducerTest, TranslationDisabledWithToggleFeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBocaTranslateToggle);
  media::SpeechRecognitionResult transcript("transcript1", /*is_final=*/true);
  std::string translated_transcript_string = "translated_transcript";
  media::SpeechRecognitionResult translated_transcript(
      translated_transcript_string, /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  translation_dispatcher_->InjectTranslationResult(
      translated_transcript_string);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  producer.OnTranscriptionResult(transcript, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            transcript);
}

TEST_F(BabelOrcaProducerTest, EnableTranslationWithToggleFeatureEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kBocaTranslateToggle);
  media::SpeechRecognitionResult transcript("transcript1", /*is_final=*/true);
  std::string translated_transcript_string = "translated_transcript";
  media::SpeechRecognitionResult translated_transcript(
      translated_transcript_string, /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  std::unique_ptr<CaptionController> caption_controller =
      GetCaptionController();
  caption_controller->SetLiveTranslateEnabled(true);
  BabelOrcaProducer producer(
      url_loader_factory_.GetSafeWeakWrapper(), std::move(speech_recognizer_),
      std::move(caption_controller), std::move(authed_client_), &data_provider,
      std::move(translator_));

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  translation_dispatcher_->InjectTranslationResult(
      translated_transcript_string);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  producer.OnTranscriptionResult(transcript, kLanguage);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            translated_transcript);
}

TEST_F(BabelOrcaProducerTest, TranslationsDontAffectSentTranscripts) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kBocaTranslateToggle);
  media::SpeechRecognitionResult transcript("transcript1", /*is_final=*/true);
  std::string translated_transcript_string = "translated_transcript";
  media::SpeechRecognitionResult translated_transcript(
      translated_transcript_string, /*is_final=*/true);
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  FakeTachyonAuthedClient* authed_client_ptr = authed_client_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  producer.OnSessionStarted();

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  pref_service_.SetString(prefs::kTranslateTargetLanguageCode,
                          kTranslationTargetLocale);
  translation_dispatcher_->InjectTranslationResult(
      translated_transcript_string);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);
  // Session translations are only relevant for consumers.  Translations
  // on the producer is controlled by the TranslationTargetLanguage
  // preference.
  producer.OnSessionCaptionConfigUpdated(/*session_captions_enabled=*/true,
                                         /*translations_enabled=*/false);
  base::OnceCallback<void(bool)> signin_cb = data_provider.TakeSigninCb();
  ASSERT_FALSE(signin_cb.is_null());
  data_provider.set_tachyon_token("tachyon_token");
  std::move(signin_cb).Run(true);

  producer.OnTranscriptionResult(transcript, kLanguage);
  authed_client_ptr->WaitForRequest();
  media::SpeechRecognitionResult sent_transcript =
      GetTranscriptFromRequest(authed_client_ptr->GetRequestString());
  // Something has gone wrong if we got the translated string here.
  EXPECT_EQ(transcript, sent_transcript);
  ASSERT_THAT(caption_controller_delegate_ptr->GetTranscriptions(),
              testing::SizeIs(1));
  EXPECT_EQ(caption_controller_delegate_ptr->GetTranscriptions()[0],
            translated_transcript);
}

TEST_F(BabelOrcaProducerTest,
       SourceLanguageSwitchTriggersOnLanguageIdentificationEvent) {
  FakeTachyonRequestDataProvider data_provider("session-id",
                                               /*tachyon_token=*/std::nullopt,
                                               "group-id", "sender@email.com");
  MockSpeechRecognizer* speech_recognizer_ptr = speech_recognizer_.get();
  FakeCaptionControllerDelegate* caption_controller_delegate_ptr =
      caption_controller_delegate_.get();
  BabelOrcaProducer producer(url_loader_factory_.GetSafeWeakWrapper(),
                             std::move(speech_recognizer_),
                             GetCaptionController(), std::move(authed_client_),
                             &data_provider, std::move(translator_));

  // Values here are arbitrary, we're just testing that the producer forwards
  // this object to the caption controller.
  media::mojom::LanguageIdentificationEventPtr language_id_event =
      media::mojom::LanguageIdentificationEvent::New(
          kTranslationTargetLocale,
          media::mojom::ConfidenceLevel::kDefaultValue);
  language_id_event->asr_switch_result =
      media::mojom::AsrSwitchResult::kSwitchSucceeded;

  EXPECT_CALL(*speech_recognizer_ptr, AddObserver(&producer)).Times(1);
  EXPECT_CALL(*speech_recognizer_ptr, Start).Times(1);
  producer.OnLocalCaptionConfigUpdated(/*local_captions_enabled=*/true);

  producer.OnLanguageIdentificationEvent(std::move(language_id_event));
  EXPECT_EQ(
      caption_controller_delegate_ptr->GetOnLanguageIdentificationEventCount(),
      1u);
}

}  // namespace
}  // namespace ash::babelorca
