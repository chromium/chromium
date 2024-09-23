// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_model.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

base::File CreateInvalidModelFile() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("model_file.tflite");
  base::File file(file_path, (base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE |
                              base::File::FLAG_CAN_DELETE_ON_CLOSE));
  EXPECT_TRUE(
      file.WriteAtCurrentPosAndCheck(base::byte_span_from_cstring("12345")));
  return file;
}

class LanguageDetectionTest : public testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(LanguageDetectionTest, ModelUnavailable) {
  LanguageDetectionModel language_detection_model;
  EXPECT_FALSE(language_detection_model.IsAvailable());
}

TEST_F(LanguageDetectionTest, EmptyFileProvided) {
  LanguageDetectionModel language_detection_model;

  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  language_detection_model.UpdateWithFile(base::File());
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileInvalid, 1);
}

TEST_F(LanguageDetectionTest, EmptyFileProvidedAsync) {
  LanguageDetectionModel language_detection_model;

  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  base::RunLoop run_loop;
  language_detection_model.UpdateWithFileAsync(base::File(),
                                               run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileInvalid, 1);
}

TEST_F(LanguageDetectionTest, UnsupportedModelFileProvided) {
  base::File file = CreateInvalidModelFile();
  LanguageDetectionModel language_detection_model;
  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  language_detection_model.UpdateWithFile(std::move(file));
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileValid, 1);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.InvalidModelFile", true, 1);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.Create.Duration", 0);
}

TEST_F(LanguageDetectionTest, UnsupportedModelFileProvidedAsync) {
  base::File file = CreateInvalidModelFile();
  LanguageDetectionModel language_detection_model;
  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  base::RunLoop run_loop;
  language_detection_model.UpdateWithFileAsync(std::move(file),
                                               run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileValid, 1);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.InvalidModelFile", true, 1);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.Create.Duration", 0);
}

TEST_F(LanguageDetectionTest, CallbackForValidFile) {
  LanguageDetectionModel language_detection_model;
  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_TRUE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  language_detection_model.UpdateWithFile(GetValidModelFile());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(language_detection_model.IsAvailable());
}

TEST_F(LanguageDetectionTest, CallbackForValidFileAsync) {
  LanguageDetectionModel language_detection_model;
  bool callback_called = false;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](bool* called, LanguageDetectionModel& model) {
        EXPECT_TRUE(model.IsAvailable());
        *called = true;
      },
      &callback_called));
  base::RunLoop run_loop;
  language_detection_model.UpdateWithFileAsync(GetValidModelFile(),
                                               run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(language_detection_model.IsAvailable());
}

class LanguageDetectionValidTest : public LanguageDetectionTest {
 public:
  LanguageDetectionValidTest()
      : language_detection_model_(GetValidLanguageModel()) {}

 protected:
  std::unique_ptr<LanguageDetectionModel> language_detection_model_;
};

TEST_F(LanguageDetectionValidTest, ValidModelFileProvided) {
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelAvailable, 1);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.InvalidModelFile", 0);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.Create.Duration", 1);
}

TEST_F(LanguageDetectionValidTest, DetectLanguageMetrics) {
  std::u16string contents = u"This is a page apparently written in English.";
  auto prediction = TopPrediction(language_detection_model_->Predict(contents));
  EXPECT_EQ("en", prediction.language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.ClassifyText.Size", contents.length(), 1);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
      contents.length(), 1);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.ClassifyText.Duration", 1);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.ClassifyText.Detected", true, 1);
}

TEST_F(LanguageDetectionValidTest, Truncation) {
  std::u16string contents = u"This is a page apparently written in English.";

  // Short string with truncation.
  {
    base::HistogramTester histogram_tester_;
    auto prediction = TopPrediction(
        language_detection_model_->Predict(contents, /*truncate=*/true));
    EXPECT_EQ("en", prediction.language);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size", contents.length(),
        1);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
        contents.length(), 1);
  }
  // Short string without truncation.
  {
    base::HistogramTester histogram_tester_;
    auto prediction = TopPrediction(
        language_detection_model_->Predict(contents, /*truncate=*/false));
    EXPECT_EQ("en", prediction.language);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size", contents.length(),
        1);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
        contents.length(), 1);
  }
  // Make a longer string. Much long than the truncation length to make sure
  // different histogram buckets are involved.
  contents += contents;
  contents += contents;
  contents += contents;
  contents += contents;
  contents += contents;
  ASSERT_GE(contents.length(), kModelTruncationLength * 4);
  // Long string with truncation.
  {
    base::HistogramTester histogram_tester_;
    auto prediction = TopPrediction(
        language_detection_model_->Predict(contents, /*truncate=*/true));
    EXPECT_EQ("en", prediction.language);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size",
        kModelTruncationLength, 1);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
        contents.length(), 1);
  }
  // Long string without truncation.
  {
    base::HistogramTester histogram_tester_;
    auto prediction = TopPrediction(
        language_detection_model_->Predict(contents, /*truncate=*/false));
    EXPECT_EQ("en", prediction.language);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size", contents.length(),
        1);
    histogram_tester_.ExpectUniqueSample(
        "LanguageDetection.TFLiteModel.ClassifyText.Size.PreTruncation",
        contents.length(), 1);
  }
}

// Regression test for https://crbug.com/1414235. This test is expecting that
// the code under test does not crash on ASan.
TEST_F(LanguageDetectionValidTest, UnalignedString) {
  std::u16string contents(1, ' ');
  language_detection_model_->Predict(contents);
}

}  // namespace language_detection
