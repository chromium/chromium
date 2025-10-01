// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/safe_browsing/content/browser/notification_content_detection/mock_safe_browsing_database_manager.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_notification_content_detection_model.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"

using ::testing::_;

namespace safe_browsing {

namespace {

const char kAllowlistedUrl[] = "https://allowlistedurl.com";
const char kNonAllowlistedUrl[] = "https://nonallowlistedurl.com";

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

  MOCK_METHOD5(Execute,
               void(blink::PlatformNotificationData& notification_data,
                    const GURL& origin,
                    bool is_allowlisted_by_user,
                    bool did_match_allowlist,
                    ModelVerdictCallback model_verdict_callback));

 private:
  std::vector<blink::PlatformNotificationData> execute_inputs_;
};

}  // namespace

class NotificationContentDetectionServiceTest
    : public ::testing::TestWithParam<bool> {
 public:
  NotificationContentDetectionServiceTest() = default;

  void SetUp() override {
    if (IsGlobalCacheListFeatureEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          kGlobalCacheListForGatingNotificationProtections);
      SetNotificationsGlobalCacheListDomainsForTesting(
          {GURL(kAllowlistedUrl).GetHost()});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kGlobalCacheListForGatingNotificationProtections);
      database_manager_ = new MockSafeBrowsingDatabaseManager();
      database_manager()->SetAllowlistLookupDetailsForUrl(GURL(kAllowlistedUrl),
                                                          /*match=*/true);
      database_manager()->SetAllowlistLookupDetailsForUrl(
          GURL(kNonAllowlistedUrl), /*match=*/false);
    }
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

  bool IsGlobalCacheListFeatureEnabled() { return GetParam(); }

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

 protected:
  base::MockCallback<NotificationContentDetectionModel::ModelVerdictCallback>
      model_verdict_callback_;

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

TEST_P(NotificationContentDetectionServiceTest, DelayedAllowlistCheckCallback) {
  // Create `PlatformNotificationData` for model prompting that we can delete
  // later in this test.
  auto* notification_data = new blink::PlatformNotificationData();
  notification_data->title = u"Notification title";

  SetUpTestNotificationContentDetectionModel();

  // Since this is an allowlisted site, `model_verdict_callback` runs before
  // the `Execute` call. By deleting `notification_data` from within the
  // `model_verdict_callback`, this test can check that `Execute` still uses
  // the original contents of `notification_data` even though the copy from
  // this test has been deleted.
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(testing::Field(&blink::PlatformNotificationData::title,
                                     u"Notification title"),
                      GURL(kAllowlistedUrl), false, true, _));
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(
          *notification_data, GURL(kAllowlistedUrl),
          /*is_allowlisted_by_user=*/false,
          /*model_verdict_callback=*/
          base::BindOnce(
              [](blink::PlatformNotificationData* notification_data,
                 bool is_suspicious,
                 std::optional<std::string>
                     serialized_content_detection_metadata) {
                delete notification_data;
              },
              notification_data));
}

TEST_P(NotificationContentDetectionServiceTest,
       CheckNotificationModelForNonAllowlistedUrl) {
  blink::PlatformNotificationData notification_data;
  SetUpTestNotificationContentDetectionModel();
  EXPECT_CALL(*notification_content_detection_model(),
              Execute(_, GURL(kNonAllowlistedUrl), false, false, _))
      .Times(1);
  EXPECT_CALL(model_verdict_callback_, Run(/*is_suspicious=*/false, _))
      .Times(0);
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(
          notification_data, GURL(kNonAllowlistedUrl),
          /*is_allowlisted_by_user=*/false, model_verdict_callback_.Get());

  // Check that histograms logging happens as expected.
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
}

TEST_P(NotificationContentDetectionServiceTest,
       CheckNotificationModelForAllowlistedUrl_FeatureParamRateIsOneHundred) {
  blink::PlatformNotificationData notification_data;
  SetUpTestNotificationContentDetectionModel();
  EXPECT_CALL(
      *notification_content_detection_model(),
      Execute(_, GURL(kAllowlistedUrl), /*is_allowlisted_by_user=*/false,
              /*did_match_allowlist=*/true, _))
      .Times(1);
  EXPECT_CALL(model_verdict_callback_,
              Run(/*is_suspicious=*/false,
                  testing::Eq("{\"is-origin-allowlisted-by-user\":false,\"is-"
                              "origin-on-global-cache-list\":true}")))
      .Times(1);
  notification_content_detection_service()
      ->MaybeCheckNotificationContentDetectionModel(
          notification_data, GURL(kAllowlistedUrl),
          /*is_allowlisted_by_user=*/false, model_verdict_callback_.Get());

  // Check that histograms logging happens as expected.
  histogram_tester().ExpectTotalCount(kAllowlistCheckLatencyHistogram, 1);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         NotificationContentDetectionServiceTest,
                         ::testing::Bool());

}  // namespace safe_browsing
