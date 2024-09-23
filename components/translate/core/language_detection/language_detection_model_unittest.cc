// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/testing/language_detection_test_utils.h"
#include "components/translate/core/common/translate_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

class LanguageDetectionModelValidTest : public testing::Test {
 public:
  LanguageDetectionModelValidTest()
      : language_detection_model_(std::make_unique<LanguageDetectionModel>(
            language_detection::GetValidLanguageModel())) {}

 protected:
  base::test::TaskEnvironment environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<LanguageDetectionModel> language_detection_model_;
};

TEST_F(LanguageDetectionModelValidTest, DetectLanguageMetrics) {
  std::u16string contents = u"This is a page apparently written in English.";
  language_detection::Prediction prediction =
      language_detection_model_->DetectLanguage(contents);
  EXPECT_EQ("en", prediction.language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.ClassifyText.HighestConfidenceLanguage",
      base::HashMetricName("en"), 1);
  histogram_tester_.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Duration", 1);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.DetectPageLanguage.Size",
      contents.length(), 1);
}

TEST_F(LanguageDetectionModelValidTest, ReliableLanguageDetermination) {
  bool is_prediction_reliable;
  float model_reliability_score = 0.0;
  std::string predicted_language;
  std::u16string contents = u"This is a page apparently written in English.";
  std::string language = language_detection_model_->DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &predicted_language,
      &is_prediction_reliable, model_reliability_score);
  EXPECT_TRUE(is_prediction_reliable);
  EXPECT_EQ("en", predicted_language);
  EXPECT_EQ(translate::kUnknownLanguageCode, language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLite.DidAttemptDetection", true, 1);
}

TEST_F(LanguageDetectionModelValidTest, LanguageDetectionAR) {
  // This test will fail if the UTF-8 is used to sample the content.
  const char* const ar_content_string =
      "متصفح الويب أو مستعرض الويب هو تطبيق برمجي لاسترجاع المعلومات "
      "عبر الإنترنت وعرضها على المستخدم. كما يعرف أنه برمجية تطبيقية "
      "لاسترجاع مصادر المعلومات على الشبكة العالمية العنكبوتية. مصادر "
      "المعلومات يحددها معرف الموارد الموحد ومن الممكن أن تحتوي صفحة "
      "الوب على الفيديو والصور أو أي محتوى آخر. الروابط التشعبية "
      "الموجودة في المصادر تمكن المستخدم من التنقل بسهولة بين "
      "المصادر ذات الصلة. "
      "المتصفح هو برنامج حاسوبي يتيح للمستخدم استعراض النصوص والصور "
      "والملفات وبعض المحتويات الأخرى المختلفة، وهذه المحتويات تكون "
      "في الغالب مخزنة في مزود إنترنت وتعرض على شكل صفحة في موقع على "
      "شبكة الإنترنت أو في شبكات محلية النصوص والصور في صفحات الموقع "
      "يمكن أن تحوي روابط لصفحات أخرى في نفس الموقع أو في مواقع أخرى. "
      "متصفح الإنترنت يتيح للمستخدم أن يصل إلى المعلومات الموجودة في "
      "المواقع بسهولة وسرعة عن طريق تتبع الروابط. على الرغم من أن "
      "المتصفحات تهدف في المقام الأول للوصول إلى الشبكة العالمية، إلا "
      "أنها أيضا يمكن أن تستخدم للوصول إلى المعلومات التي توفرها خدمة "
      "الإنترنت خادم الإنترنت في الشبكات الخاصة أو الملفات في انظمة الملفات "
      "متصفحات الإنترنت الرئيسية حاليًا هي مايكروسوفت إيدج، وموزيلا فيرفكس، "
      "وجوجل كروم، وأبل سفاري، وأوبرا.";

  std::u16string contents = base::UTF8ToUTF16(ar_content_string);
  bool is_prediction_reliable;
  float model_reliability_score = 0.0;
  std::string predicted_language;
  std::string language = language_detection_model_->DeterminePageLanguage(
      std::string("ar"), std::string(), contents, &predicted_language,
      &is_prediction_reliable, model_reliability_score);
  EXPECT_TRUE(is_prediction_reliable);
  EXPECT_EQ("ar", predicted_language);
  EXPECT_EQ("ar", language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLite.DidAttemptDetection", true, 1);
}

TEST_F(LanguageDetectionModelValidTest, UnreliableLanguageDetermination) {
  bool is_prediction_reliable;
  float model_reliability_score = 0.0;
  std::string predicted_language;
  std::u16string contents = u"e";
  std::string language = language_detection_model_->DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &predicted_language,
      &is_prediction_reliable, model_reliability_score);
  EXPECT_FALSE(is_prediction_reliable);
  EXPECT_EQ(translate::kUnknownLanguageCode, predicted_language);
  // Rely on the provided language code if the mode is unreliable.
  EXPECT_EQ("ja", language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLite.DidAttemptDetection", true, 1);
}

TEST_F(LanguageDetectionModelValidTest, LongTextLanguageDetemination) {
  bool is_prediction_reliable;
  float model_reliability_score = 0.0;
  std::string predicted_language;
  const char* const zh_content_string =
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "对于要提交的图书 我确认 我是版权所有者或已得到版权所有者的授权 "
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "This is a page apparently written in English."
      "This is a page apparently written in English."
      "This is a page apparently written in English."
      "要更改您的国家 地区 请在此表的最上端更改您的"
      "产品的简报和公告 提交该申请后无法进行更改 请确认您的选择是正确的 "
      "要更改您的国家 地区 请在此表的最上端更改您的";

  std::u16string contents = base::UTF8ToUTF16(zh_content_string);
  EXPECT_GE(contents.length(), 250u * 3u);
  std::string language = language_detection_model_->DeterminePageLanguage(
      std::string("ja"), std::string(), contents, &predicted_language,
      &is_prediction_reliable, model_reliability_score);
  EXPECT_TRUE(is_prediction_reliable);
  EXPECT_EQ("zh-CN", predicted_language);
  EXPECT_EQ(translate::kUnknownLanguageCode, language);
  histogram_tester_.ExpectUniqueSample(
      "LanguageDetection.TFLite.DidAttemptDetection", true, 1);
}

}  // namespace translate
