// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
#include "components/soda/mock_soda_installer.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_recognition_context.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {
namespace {

class CrosSpeechRecognitionRecognizerImplTest : public testing::Test {
  void SetUp() override {}
};

TEST_F(CrosSpeechRecognitionRecognizerImplTest, EmptyLangs) {
  chromeos::machine_learning::mojom::SodaMultilangConfigPtr expected =
      chromeos::machine_learning::mojom::SodaMultilangConfig::New();
  auto actual =
      CrosSpeechRecognitionRecognizerImpl::AddLiveCaptionLanguagesToConfig(
          "en-US", base::flat_map<std::string, base::FilePath>(),
          {"en-US", "en-AU"});
  EXPECT_EQ(expected, actual);
}

TEST_F(CrosSpeechRecognitionRecognizerImplTest, FilledLangs) {
  chromeos::machine_learning::mojom::SodaMultilangConfigPtr expected =
      chromeos::machine_learning::mojom::SodaMultilangConfig::New();
  base::flat_map<std::string, base::FilePath> config_paths;
  config_paths["en-AU"] = base::FilePath::FromASCII("/fake/path/aus");
  config_paths["en-US"] = base::FilePath::FromASCII("/fake/path/usa");
  config_paths["es-US"] = base::FilePath::FromASCII("/fake/path/espusa");
  config_paths["es-ES"] = base::FilePath::FromASCII("/fake/path/espesp");
  config_paths["fr-FR"] = base::FilePath::FromASCII("/fake/path/frafra");

  auto actual =
      CrosSpeechRecognitionRecognizerImpl::AddLiveCaptionLanguagesToConfig(
          "en-US", config_paths, {"en-US", "es-US", "fr-FR"});
  expected->locale_to_language_pack_map["es-US"] = "/fake/path/espusa";
  expected->locale_to_language_pack_map["fr-FR"] = "/fake/path/frafra";
  EXPECT_EQ(expected, actual);
}

class FakeSpeechRecognitionRecognizerClient
    : public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  FakeSpeechRecognitionRecognizerClient() = default;

  FakeSpeechRecognitionRecognizerClient(
      const FakeSpeechRecognitionRecognizerClient&) = delete;
  FakeSpeechRecognitionRecognizerClient& operator=(
      const FakeSpeechRecognitionRecognizerClient&) = delete;

  ~FakeSpeechRecognitionRecognizerClient() override = default;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}
};

class FakeMachineLearningServiceConnection
    : public chromeos::machine_learning::FakeServiceConnectionImpl {
 public:
  explicit FakeMachineLearningServiceConnection(
      base::OnceCallback<void(chromeos::machine_learning::mojom::SodaConfigPtr
                                  soda_config)> soda_config_cb)
      : soda_config_cb_(std::move(soda_config_cb)) {}

  FakeMachineLearningServiceConnection(
      const FakeMachineLearningServiceConnection&) = delete;
  FakeMachineLearningServiceConnection& operator=(
      const FakeMachineLearningServiceConnection&) = delete;

  ~FakeMachineLearningServiceConnection() override = default;

  void LoadSpeechRecognizer(
      chromeos::machine_learning::mojom::SodaConfigPtr soda_config,
      mojo::PendingRemote<chromeos::machine_learning::mojom::SodaClient>
          soda_client,
      mojo::PendingReceiver<chromeos::machine_learning::mojom::SodaRecognizer>
          soda_recognizer,
      chromeos::machine_learning::mojom::MachineLearningService::
          LoadSpeechRecognizerCallback callback) override {
    std::move(soda_config_cb_).Run(std::move(soda_config));
    std::move(callback).Run(
        chromeos::machine_learning::mojom::LoadModelResult::OK);
  }

 private:
  base::OnceCallback<void(
      chromeos::machine_learning::mojom::SodaConfigPtr soda_config)>
      soda_config_cb_;
};

using CrosSpeechRecognitionRecognizerImplMaskOffensiveWordsTest =
    testing::TestWithParam<bool>;

TEST_P(CrosSpeechRecognitionRecognizerImplMaskOffensiveWordsTest,
       SetMaskOffensiveWords) {
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList features{
      ash::features::kOnDeviceSpeechRecognition};
  base::test::TestFuture<chromeos::machine_learning::mojom::SodaConfigPtr>
      test_future;
  FakeMachineLearningServiceConnection fake_service_connection(
      test_future.GetCallback());
  fake_service_connection.Initialize();
  chromeos::machine_learning::ServiceConnection::
      UseFakeServiceConnectionForTesting(&fake_service_connection);
  speech::MockSodaInstaller soda_installer;
  FakeSpeechRecognitionRecognizerClient recognizer_client;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      &recognizer_client};
  CrosSpeechRecognitionRecognizerImpl recognizer(
      receiver_.BindNewPipeAndPassRemote(),
      media::mojom::SpeechRecognitionOptions::New(
          /*recognition_mode*/ media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/false, /*language=*/"en-US",
          /*is_server_based=*/false,
          media::mojom::RecognizerClientType::kLiveCaption),
      base::FilePath(), base::flat_map<std::string, base::FilePath>(), "en-US",
      /*mask_offensive_words=*/GetParam());

  recognizer.AddAudio(
      media::mojom::AudioDataS16::New(/*channel_count=*/1,
                                      /*sample_rate=*/1,
                                      /*frame_count=*/1,
                                      /*data=*/std::vector<int16_t>{1}));
  chromeos::machine_learning::mojom::SodaConfigPtr config = test_future.Take();

  EXPECT_EQ(config->mask_offensive_words, GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    CrosSpeechRecognitionRecognizerImplMaskOffensiveWordsTestSuite,
    CrosSpeechRecognitionRecognizerImplMaskOffensiveWordsTest,
    testing::Bool());

}  // namespace
}  // namespace speech
