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
#include "components/language_detection/core/constants.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language_detection {

namespace {
// Pads out `s` with spaces until it is `len` long.
void pad(std::u16string& s, size_t len) {
  while (s.length() < len) {
    s += u" ";
  }
}

}  // namespace

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

  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFile(base::File());
  run_loop.Run();
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileInvalid, 1);
}

TEST_F(LanguageDetectionTest, EmptyFileProvidedAsync) {
  LanguageDetectionModel language_detection_model;

  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFileAsync(base::File(), base::DoNothing());
  run_loop.Run();
  EXPECT_FALSE(language_detection_model.IsAvailable());
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      LanguageDetectionModelState::kModelFileInvalid, 1);
}

TEST_F(LanguageDetectionTest, UnsupportedModelFileProvided) {
  base::File file = CreateInvalidModelFile();
  LanguageDetectionModel language_detection_model;
  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFile(std::move(file));
  run_loop.Run();
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
  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_FALSE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFileAsync(std::move(file),
                                               base::DoNothing());
  run_loop.Run();
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
  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_TRUE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFile(GetValidModelFile());
  run_loop.Run();
  EXPECT_TRUE(language_detection_model.IsAvailable());
}

TEST_F(LanguageDetectionTest, CallbackForValidFileAsync) {
  LanguageDetectionModel language_detection_model;
  base::RunLoop run_loop;
  language_detection_model.AddOnModelLoadedCallback(base::BindOnce(
      [](base::OnceClosure callback, LanguageDetectionModel& model) {
        EXPECT_TRUE(model.IsAvailable());
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  language_detection_model.UpdateWithFileAsync(GetValidModelFile(),
                                               base::DoNothing());
  run_loop.Run();
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

// This directly tests the sampling method for longer strings. We have 1 piece
// of text that is unambiguously EN and one that is mixes AR and ZH. We combine
// these so that they become the samples. Since EN is unambiguous, the result
// should be EN.
// This test is highly dependent on the sampling implementation.
// See https://crbug.com/378011996
TEST_F(LanguageDetectionValidTest, DetectWithSampling) {
  // If this changes, this test needs to be rewritten.
  ASSERT_EQ(LanguageDetectionModel::kNumTextSamples, 3);
  std::string predicted_language;
  std::u16string en_sample = u"This is a page apparently written in English.";
  pad(en_sample, LanguageDetectionModel::kTextSampleLength);
  std::u16string ar_zh_sample =
      u"متصفح الويب أو مستعرض الويب هو تطبيق برمجي لاسترجاع المعلومات "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 ";
  pad(ar_zh_sample, LanguageDetectionModel::kTextSampleLength);

  ASSERT_EQ(en_sample.length(), LanguageDetectionModel::kTextSampleLength);
  ASSERT_EQ(ar_zh_sample.length(), LanguageDetectionModel::kTextSampleLength);

  // Test against strings where the EN string in the `pos`th sample.
  for (int pos = 0; pos < 3; pos++) {
    SCOPED_TRACE(pos);
    std::u16string s1 = pos == 0 ? en_sample : ar_zh_sample;
    std::u16string s2 = pos == 1 ? en_sample : ar_zh_sample;
    std::u16string s3 = pos == 2 ? en_sample : ar_zh_sample;
    // Construct a string that starts with s1, has s2 starting at mid-point
    // and then ends with s3. The string will of length `6*kTextSampleLength`.
    std::u16string contents = s1;
    pad(contents, LanguageDetectionModel::kTextSampleLength * 3);
    contents += s2;
    pad(contents, LanguageDetectionModel::kTextSampleLength * 5);
    contents += s3;
    ASSERT_EQ(contents.length(), 6 * LanguageDetectionModel::kTextSampleLength);
    language_detection::Prediction prediction =
        language_detection_model_->PredictTopLanguageWithSamples(contents);
    EXPECT_EQ("en", prediction.language);
  }
}

TEST_F(LanguageDetectionValidTest, PredictWithScanEmptyInput) {
  std::u16string empty_string;
  std::vector<Prediction> results_empty =
      language_detection_model_->PredictWithScan(empty_string);
  ASSERT_EQ(TopPrediction(results_empty).language, kUnknownLanguageCode);
}

TEST_F(LanguageDetectionValidTest, PredictWithScan) {
  std::string predicted_language;
  std::u16string en_sample = u"This is a page apparently written in English.";
  pad(en_sample, LanguageDetectionModel::kScanWindowSize);
  std::u16string ar_sample =
      u"متصفح الويب أو مستعرض الويب هو تطبيق برمجي لاسترجاع المعلومات ";
  pad(ar_sample, LanguageDetectionModel::kScanWindowSize);
  std::u16string zh_sample =
      u"产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 ";
  pad(zh_sample, LanguageDetectionModel::kScanWindowSize);

  ASSERT_EQ(en_sample.length(), LanguageDetectionModel::kScanWindowSize);
  ASSERT_EQ(ar_sample.length(), LanguageDetectionModel::kScanWindowSize);
  ASSERT_EQ(zh_sample.length(), LanguageDetectionModel::kScanWindowSize);

  std::u16string final_sample = en_sample + ar_sample + zh_sample;
  // Scanning over the concatencated sample shall result in a mean value of the
  // three detection results.
  std::vector<Prediction> results_en =
      language_detection_model_->PredictWithScan(en_sample);
  ASSERT_EQ(TopPrediction(results_en).language, "en");
  std::vector<Prediction> results_ar =
      language_detection_model_->PredictWithScan(ar_sample);
  ASSERT_EQ(TopPrediction(results_ar).language, "ar");
  std::vector<Prediction> results_zh =
      language_detection_model_->PredictWithScan(zh_sample);
  ASSERT_EQ(TopPrediction(results_zh).language, "zh");
  std::map<std::string, float> confidence_sum;
  for (auto&& results : {results_en, results_ar, results_zh}) {
    for (auto&& prediction : results) {
      confidence_sum[prediction.language] += prediction.score;
    }
  }
  std::vector<Prediction> results_final =
      language_detection_model_->PredictWithScan(final_sample);

  // The prediction confidence is the mean value of confidence of the
  // corresponding language in the three samples.
  for (auto&& prediction : results_final) {
    ASSERT_TRUE(confidence_sum.contains(prediction.language));
    ASSERT_GE(prediction.score * 3,
              confidence_sum[prediction.language] - 0.0001);
    ASSERT_LE(prediction.score * 3,
              confidence_sum[prediction.language] + 0.0001);
  }
}

TEST_F(LanguageDetectionValidTest, Truncation) {
  std::u16string contents = u"This is a page apparently written in English.";

  // Make a longer string. Much long than the truncation length to make sure
  // different histogram buckets are involved.
  contents += contents;
  contents += contents;
  contents += contents;
  contents += contents;
  contents += contents;
  ASSERT_GE(contents.length(), kModelTruncationLength * 4);
  // Long string with truncation.
  base::HistogramTester histogram_tester_;
  auto prediction = TopPrediction(language_detection_model_->Predict(contents));
  EXPECT_EQ("en", prediction.language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.ClassifyText.Size", kModelTruncationLength,
      1);
}

// Regression test for https://crbug.com/1414235. This test is expecting that
// the code under test does not crash on ASan.
TEST_F(LanguageDetectionValidTest, UnalignedString) {
  std::u16string contents(1, ' ');
  language_detection_model_->Predict(contents);
}

}  // namespace language_detection
