// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/safe_browsing/content/browser/notification_content_detection/mock_safe_browsing_database_manager.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_notification_content_detection_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

namespace safe_browsing {

namespace {

base::FilePath GetValidModelFile() {
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("safe_browsing")
                        .AppendASCII("doesnt_exist_unused.tflite");
  return model_file_path;
}

class MockNotificationContentDetectionModel
    : public TestNotificationContentDetectionModel {
 public:
  MockNotificationContentDetectionModel(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      content::BrowserContext* browser_context)
      : TestNotificationContentDetectionModel(model_provider,
                                              background_task_runner,
                                              browser_context) {}

  MOCK_METHOD3(Execute,
               void(blink::PlatformNotificationData& notification_data,
                    const GURL& origin,
                    bool did_match_allowlist));

 private:
  std::vector<blink::PlatformNotificationData> execute_inputs_;
};

}  // namespace

class NotificationContentDetectionServiceTest : public ::testing::Test {
 public:
  NotificationContentDetectionServiceTest() = default;

  void SetUp() override {
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    model_observer_tracker_ = std::make_unique<TestModelObserverTracker>();
    notification_content_detection_service_ =
        std::make_unique<NotificationContentDetectionService>(
            model_observer_tracker_.get(),
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
            database_manager_, &browser_context_);

    // Update service with test model.
    SetUpTestNotificationContentDetectionModel();
  }

  void TearDown() override {
    notification_content_detection_model_ = nullptr;
    notification_content_detection_service_.reset();
    model_observer_tracker_.reset();
  }

  void SetUpTestNotificationContentDetectionModel() {
    auto test_notification_content_detection_model =
        std::make_unique<MockNotificationContentDetectionModel>(
            model_observer_tracker_.get(),
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
            &browser_context_);

    base::FilePath model_file_path = GetValidModelFile();
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetVersion(1)
            .SetModelFilePath(model_file_path)
            .Build();
    test_notification_content_detection_model->OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_NOTIFICATION_CONTENT_DETECTION,
        *model_info);

    notification_content_detection_model_ =
        test_notification_content_detection_model.get();

    notification_content_detection_service()->SetModelForTesting(
        std::move(test_notification_content_detection_model));
  }

  void SetAllowistedSiteSampleRateToOneHundred() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kOnDeviceNotificationContentDetectionModel,
        {{"OnDeviceNotificationContentDetectionModelAllowlistSamplingRate",
          /* Override to 100% to make sure it will be sampled. */
          "100"}});
  }

  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager() {
    return database_manager_;
  }

  NotificationContentDetectionService* notification_content_detection_service()
      const {
    return notification_content_detection_service_.get();
  }

  MockNotificationContentDetectionModel*
  notification_content_detection_model() {
    return notification_content_detection_model_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<TestModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<NotificationContentDetectionService>
      notification_content_detection_service_;
  raw_ptr<MockNotificationContentDetectionModel>
      notification_content_detection_model_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(NotificationContentDetectionServiceTest, DelayedAllowlistCheckCallback) {
  // Setup non-allowlisted URL.
  const GURL origin("not_allowlisted_url.com");
  database_manager()->SetAllowlistLookupDetailsForUrl(origin, /*match=*/false);

  // Create `PlatformNotificationData` for model prompting that we can delete
  // later in this test.
  auto* notification_data = new blink::PlatformNotificationData();
  notification_data->title = u"Notification title";

  // Delay calling the callback passed to `CheckUrlForHighConfidenceAllowlist`
  // so that this test can verify properly passed ownership of the
  // `PlatformNotificationData` parameter.
  database_manager()->SetCallbackToDelayed(origin);
  SetUpTestNotificationContentDetectionModel();
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(*notification_data, origin);

  // Deleting `notification_data` should still result in successful callback
  // execution with a non-empty title.
  delete notification_data;
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(testing::Field(&blink::PlatformNotificationData::title,
                                     u"Notification title"),
                      origin, false))
      .Times(1);
  database_manager()->RestartDelayedCallback(origin);
}

TEST_F(NotificationContentDetectionServiceTest,
       CheckNotificationModelForNonAllowlistedUrl) {
  // Setup non-allowlisted URL.
  const GURL origin("nonallowlisted_url.com");
  database_manager()->SetAllowlistLookupDetailsForUrl(origin, /*match=*/false);

  blink::PlatformNotificationData notification_data;
  SetUpTestNotificationContentDetectionModel();
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(testing::_, origin, false))
      .Times(1);
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(notification_data, origin);

  // Check that histograms logging happens as expected.
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
}

TEST_F(NotificationContentDetectionServiceTest,
       CheckNotificationModelForAllowlistedUrl_FeatureParamRateIsOneHundred) {
  SetAllowistedSiteSampleRateToOneHundred();
  // Setup allowlisted URL.
  const GURL origin("allowlisted_url.com");
  database_manager()->SetAllowlistLookupDetailsForUrl(origin, /*match=*/true);

  blink::PlatformNotificationData notification_data;
  SetUpTestNotificationContentDetectionModel();
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(testing::_, origin, true))
      .Times(1);
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(notification_data, origin);

  // Check that histograms logging happens as expected.
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
}

TEST_F(NotificationContentDetectionServiceTest,
       DontCheckNotificationModelForAllowlistedUrl_ByDefault) {
  // Setup allowlisted URL.
  const GURL origin("allowlisted_url.com");
  database_manager()->SetAllowlistLookupDetailsForUrl(origin, /*match=*/true);

  // Model should not be checked.
  blink::PlatformNotificationData notification_data;
  SetUpTestNotificationContentDetectionModel();
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(testing::_, testing::_, testing::_))
      .Times(0);
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(notification_data, origin);

  // Check that histograms logging happens as expected.
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
}

}  // namespace safe_browsing
