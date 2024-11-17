// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/language/ios/browser/ios_language_detection_tab_helper.h"

#import "base/files/file_util.h"
#import "base/metrics/metrics_hashes.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/language_detection/core/language_detection_model.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/common/translate_util.h"
#import "components/translate/core/language_detection/language_detection_model.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace language {

class IOSLanguageDetectionTabHelperTest : public PlatformTest {
 public:
  IOSLanguageDetectionTabHelperTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled}, {});

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web::ContentWorld content_world =
        language::LanguageDetectionJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world, std::move(frames_manager));

    pref_service_.registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_, /*url_language_histogram=*/nullptr, &model_,
        &pref_service_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple pref_service_;
  translate::LanguageDetectionModel model_{
      std::make_unique<language_detection::LanguageDetectionModel>()};
  web::FakeWebState web_state_;
};

base::File GetValidModelFile() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("translate")
                                       .AppendASCII("valid_model.tflite");
  base::File file(model_file_path,
                  (base::File::FLAG_OPEN | base::File::FLAG_READ));
  return file;
}

TEST_F(IOSLanguageDetectionTabHelperTest,
       TFLiteLanguageDetectionDurationRecorded) {
  language::IOSLanguageDetectionTabHelper* language_detection_tab_helper =
      language::IOSLanguageDetectionTabHelper::FromWebState(&web_state_);

  histogram_tester_.ExpectTotalCount(
      "Translate.LanguageDetection.TFLiteModelEvaluationDuration", 0);

  base::RunLoop run_loop;
  model_.UpdateWithFileAsync(GetValidModelFile(), run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(model_.IsAvailable());

  base::Value text_content("hello world");
  language_detection_tab_helper->OnTextRetrieved(true, "en", "en", GURL(""),
                                                 &text_content);

  histogram_tester_.ExpectTotalCount(
      "Translate.LanguageDetection.TFLiteModelEvaluationDuration", 1);
}

}  // namespace language
