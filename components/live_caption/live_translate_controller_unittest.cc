// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_translate_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "components/live_caption/features.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {

class MockTranslationDispatcher : public TranslationDispatcher {
 public:
  MockTranslationDispatcher() = default;
  ~MockTranslationDispatcher() override = default;

  MOCK_METHOD(void,
              GetTranslation,
              (absl::string_view result,
               absl::string_view source_language,
               absl::string_view target_language,
               TranslateEventCallback callback),
              (override));
};

class LiveTranslateControllerTest : public testing::Test {
 public:
  LiveTranslateControllerTest() = default;
  ~LiveTranslateControllerTest() override = default;

  void SetUp() override {
    LiveTranslateController::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(LiveTranslateControllerTest,
       GetTranslationRecordsMetrics_GoogleApi_Success) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      live_caption::kLiveCaptionOnDeviceTranslation);

  base::HistogramTester histogram_tester;
  auto mock_translation_dispatcher =
      std::make_unique<testing::StrictMock<MockTranslationDispatcher>>();
  auto* translation_dispatcher_ptr = mock_translation_dispatcher.get();
  LiveTranslateController controller(&prefs_,
                                     std::move(mock_translation_dispatcher));

  // Verify that the controller forwards translation requests to the underlying
  // dispatcher and successfully records true (success) in the Google API UMA
  // histogram.
  EXPECT_CALL(*translation_dispatcher_ptr,
              GetTranslation("hello", "en", "es", testing::_))
      .WillOnce([](absl::string_view result, absl::string_view source_language,
                   absl::string_view target_language,
                   TranslateEventCallback callback) {
        std::move(callback).Run(base::ok("hola"));
      });

  base::MockCallback<TranslateEventCallback> translate_callback;
  EXPECT_CALL(translate_callback, Run(testing::_)).Times(1);

  controller.GetTranslation("hello", "en", "es", translate_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Accessibility.LiveTranslate.GoogleApiTranslation.Result",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(LiveTranslateControllerTest,
       GetTranslationRecordsMetrics_OnDevice_Failure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      live_caption::kLiveCaptionOnDeviceTranslation);

  base::HistogramTester histogram_tester;
  auto mock_translation_dispatcher =
      std::make_unique<testing::StrictMock<MockTranslationDispatcher>>();
  auto* translation_dispatcher_ptr = mock_translation_dispatcher.get();
  LiveTranslateController controller(&prefs_,
                                     std::move(mock_translation_dispatcher));

  // Controller calls dispatcher and successfully records false (failure) in the
  // On-Device UMA histogram.
  EXPECT_CALL(*translation_dispatcher_ptr,
              GetTranslation("hello", "en", "es", testing::_))
      .WillOnce([](absl::string_view result, absl::string_view source_language,
                   absl::string_view target_language,
                   TranslateEventCallback callback) {
        std::move(callback).Run(base::unexpected("translation failed"));
      });

  base::MockCallback<TranslateEventCallback> translate_callback;
  EXPECT_CALL(translate_callback, Run(testing::_)).Times(1);

  controller.GetTranslation("hello", "en", "es", translate_callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Accessibility.LiveTranslate.OnDeviceTranslation.Result",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

}  // namespace captions
