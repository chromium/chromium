// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

base::File CreateValidModelFile() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("model_file.tflite");
  base::File file(file_path, (base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE |
                              base::File::FLAG_CAN_DELETE_ON_CLOSE));
  EXPECT_TRUE(file.WriteAtCurrentPos("12345", 5));
  return file;
}

TEST(LanguageDetectionModelTest, ModelUnavailable) {
  LanguageDetectionModel language_detection_model;
  EXPECT_FALSE(language_detection_model.IsAvailable());
}

TEST(LanguageDetectionModelTest, InvalidFileProvided) {
  base::HistogramTester histogram_tester;
  LanguageDetectionModel language_detection_model;
  language_detection_model.UpdateWithFile(base::File());

  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileInvalid, 1);
}

TEST(LanguageDetectionModelTest, ValidFileProvided) {
  base::HistogramTester histogram_tester;

  base::File file = CreateValidModelFile();
  LanguageDetectionModel language_detection_model;
  language_detection_model.UpdateWithFile(std::move(file));
  EXPECT_TRUE(language_detection_model.IsAvailable());
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileValidAndMemoryMapped, 1);
}

TEST(LanguageDetectionModelTest, DeterminePageLanguage) {
  base::HistogramTester histogram_tester;
  base::File file = CreateValidModelFile();
  LanguageDetectionModel language_detection_model;
  language_detection_model.UpdateWithFile(std::move(file));
  EXPECT_TRUE(language_detection_model.IsAvailable());

  bool is_prediction_reliable;
  std::string predicted_language;
  base::string16 contents =
      base::ASCIIToUTF16("This is a page apparently written in English.");
  std::string language = language_detection_model.DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &predicted_language,
      &is_prediction_reliable);
  EXPECT_FALSE(is_prediction_reliable);
  EXPECT_EQ(translate::kUnknownLanguageCode, predicted_language);
  EXPECT_EQ(translate::kUnknownLanguageCode, language);
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLite.DidDetectPageLanguage", true, 1);
}

}  // namespace translate