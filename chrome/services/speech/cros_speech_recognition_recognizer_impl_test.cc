// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"

#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrosSpeechRecognitionRecognizerImplTest : public testing::Test {
  void SetUp() override {}
};

TEST_F(CrosSpeechRecognitionRecognizerImplTest, EmptyLangs) {
  chromeos::machine_learning::mojom::SodaMultilangConfigPtr expected =
      chromeos::machine_learning::mojom::SodaMultilangConfig::New();
  auto actual = speech::CrosSpeechRecognitionRecognizerImpl::
      AddLiveCaptionLanguagesToConfig(
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

  auto actual = speech::CrosSpeechRecognitionRecognizerImpl::
      AddLiveCaptionLanguagesToConfig("en-US", config_paths,
                                      {"en-US", "es-US", "fr-FR"});
  expected->locale_to_language_pack_map["es-US"] = "/fake/path/espusa";
  expected->locale_to_language_pack_map["fr-FR"] = "/fake/path/frafra";
  EXPECT_EQ(expected, actual);
}
