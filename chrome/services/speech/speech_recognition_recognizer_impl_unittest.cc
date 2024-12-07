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
using ::testing::Return;

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

  void SetUp() override {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    media::mojom::SpeechRecognitionOptionsPtr options =
        media::mojom::SpeechRecognitionOptions::New();
    config_paths_[kPrimaryLanguageName] = base::FilePath();

    recognizer_ = std::make_unique<SpeechRecognitionRecognizerImpl>(
        std::move(remote), std::move(options), base::FilePath(), config_paths_,
        kPrimaryLanguageName, /*mask_offensive_words=*/true);

    auto soda_client = std::make_unique<NiceMock<::soda::MockSodaClient>>();
    soda_client_ = soda_client.get();
    recognizer_->SetSodaClientForTesting(std::move(soda_client));
  }

  void TearDown() override { soda_client_ = nullptr; }

  base::flat_map<std::string, base::FilePath> config_paths() {
    return config_paths_;
  }
  SpeechRecognitionRecognizerImpl* recognizer() { return recognizer_.get(); }
  ::soda::MockSodaClient* soda_client() { return soda_client_; }

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      this};
  base::flat_map<std::string, base::FilePath> config_paths_;
  std::unique_ptr<SpeechRecognitionRecognizerImpl> recognizer_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<::soda::MockSodaClient> soda_client_;
};

TEST_F(SpeechRecognitionRecognizerImplTest, OnLanguagePackInstalledTest) {
  EXPECT_CALL(*soda_client(), Reset(_, _, _));
  recognizer()->OnLanguagePackInstalled(config_paths());
}

}  // namespace speech
