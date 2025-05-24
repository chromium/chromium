// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "chrome/services/speech/soda/mock_soda_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;

namespace speech {

namespace {

constexpr char kPrimaryLanguageName[] = "en-US";

}  // namespace

class SpeechRecognitionRecognizerImplTest
    : public testing::Test,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  SpeechRecognitionRecognizerImplTest() = default;
  SpeechRecognitionRecognizerImplTest(
      const SpeechRecognitionRecognizerImplTest&) = delete;
  SpeechRecognitionRecognizerImplTest& operator=(
      const SpeechRecognitionRecognizerImplTest&) = delete;
  ~SpeechRecognitionRecognizerImplTest() override = default;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}

  void SetUp() override {}

  std::unique_ptr<SpeechRecognitionRecognizerImpl> CreateRecognizer(
      media::mojom::SpeechRecognitionOptionsPtr options) {
    return std::make_unique<SpeechRecognitionRecognizerImpl>(
        receiver_.BindNewPipeAndPassRemote(), std::move(options),
        base::FilePath(), config_paths_, kPrimaryLanguageName,
        /*mask_offensive_words=*/true);
  }

  media::mojom::SpeechRecognitionOptionsPtr CreateOptions(
      std::optional<media::SpeechRecognitionRecognitionContext>
          recognition_context = std::nullopt) {
    media::mojom::SpeechRecognitionOptionsPtr options =
        media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->enable_formatting = false;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->skip_continuously_empty_audio = true;
    if (recognition_context) {
      options->recognition_context = std::move(*recognition_context);
    }
    return options;
  }

  base::flat_map<std::string, base::FilePath> config_paths() {
    return config_paths_;
  }

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      this};
  base::flat_map<std::string, base::FilePath> config_paths_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SpeechRecognitionRecognizerImplTest, OnLanguagePackInstalledTest) {
  auto recognizer = CreateRecognizer(CreateOptions());
  auto soda_client = std::make_unique<NiceMock<::soda::MockSodaClient>>();
  auto* soda_client_ptr = soda_client.get();
  recognizer->SetSodaClientForTesting(std::move(soda_client));

  EXPECT_CALL(*soda_client_ptr, Reset(_, _, _));
  recognizer->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_FALSE(config->enable_formatting());
  EXPECT_TRUE(config->mask_offensive_words());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       SpeechRecognitionRecognitionContextTest) {
  std::vector<media::SpeechRecognitionPhrase> phrases;
  phrases.emplace_back("test phrase", 2.0);

  auto recognizer = CreateRecognizer(
      CreateOptions(media::SpeechRecognitionRecognitionContext(phrases)));
  recognizer->SetSodaClientForTesting(
      std::make_unique<NiceMock<::soda::MockSodaClient>>());
  recognizer->OnLanguagePackInstalled(config_paths());

  auto context =
      recognizer->GetExtendedSodaConfigMsgForTesting()->recognition_context();
  EXPECT_EQ(1, context.context().size());
  auto context_input = context.context().Get(0);
  EXPECT_EQ("android-speech-api-generic-phrases", context_input.name());
  EXPECT_EQ(1, context_input.phrases().phrase().size());
  auto phrase = context_input.phrases().phrase().Get(0);
  EXPECT_EQ("test phrase", phrase.phrase());
  EXPECT_EQ(2.0, phrase.boost());
}

TEST_F(SpeechRecognitionRecognizerImplTest, UpdateRecognitionContextTest) {
  auto recognizer = CreateRecognizer(CreateOptions());
  auto soda_client = std::make_unique<NiceMock<::soda::MockSodaClient>>();
  auto* soda_client_ptr = soda_client.get();
  recognizer->SetSodaClientForTesting(std::move(soda_client));

  media::SpeechRecognitionRecognitionContext context;
  context.phrases.emplace_back("test phrase", 2.0);
  EXPECT_CALL(*soda_client_ptr, UpdateRecognitionContext(_));
  recognizer->UpdateRecognitionContext(context);
}

}  // namespace speech
