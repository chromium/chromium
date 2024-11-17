// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_model_observer_tracker.h"
#include "components/safe_browsing/content/browser/notification_content_detection/test_notification_content_detection_model.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr int kModelVersion = 123;
constexpr double kSuspiciousScoreTestValue = 0.59;

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

struct NotificationContentDetectionModelTestCase {
  std::u16string title;
  std::u16string body;
  std::vector<std::u16string> action_titles;
  std::string expected_input;
};

}  // namespace

class NotificationContentDetectionModelTest : public testing::Test {
 public:
  NotificationContentDetectionModelTest() = default;
  ~NotificationContentDetectionModelTest() override = default;

  void SetUp() override {
    model_observer_tracker_ = std::make_unique<TestModelObserverTracker>();
    notification_content_detection_model_ =
        std::make_unique<TestNotificationContentDetectionModel>(
            model_observer_tracker_.get(),
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
            &browser_context_);
  }

  void TearDown() override {
    notification_content_detection_model_.reset();
    model_observer_tracker_.reset();
  }

  void SendModelToNotificationContentDetectionModel() {
    base::FilePath model_file_path = GetValidModelFile();
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetVersion(kModelVersion)
            .SetModelFilePath(model_file_path)
            .Build();
    notification_content_detection_model()->OnModelUpdated(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_NOTIFICATION_CONTENT_DETECTION,
        *model_info);
  }

  TestNotificationContentDetectionModel* notification_content_detection_model()
      const {
    return notification_content_detection_model_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<TestModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<TestNotificationContentDetectionModel>
      notification_content_detection_model_;
  base::HistogramTester histogram_tester_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  permissions::TestPermissionsClient client_;
};

TEST_F(NotificationContentDetectionModelTest, CheckModelUpdateAvailability) {
  // Notification content detection model has not been loaded yet, so no info
  // or version should be available yet.
  EXPECT_FALSE(
      notification_content_detection_model()->GetModelInfo().has_value());

  // Update with a notification content detection model.
  SendModelToNotificationContentDetectionModel();

  // Now that there's a notification content detection model, should have model
  // info available.
  EXPECT_TRUE(
      notification_content_detection_model()->GetModelInfo().has_value());
  EXPECT_EQ(
      notification_content_detection_model()->GetModelInfo()->GetVersion(),
      kModelVersion);

  // Expect relevant OptimizationGuide histograms to be logged.
  histogram_tester().ExpectTotalCount(
      "OptimizationGuide.ModelHandler.HandlerCreatedToModelAvailable."
      "NotificationContentDetection",
      1);
}

TEST_F(NotificationContentDetectionModelTest, LogNotificationSuspiciousScore) {
  // Update with a notification content detection model.
  SendModelToNotificationContentDetectionModel();

  // Create tests cases for checking formatting of model input.
  std::vector<NotificationContentDetectionModelTestCase> tests = {
      {u"Notification title",
       u"Notification body",
       {},
       "Notification title,Notification body,"},
      {u"title", u" body ", {u"action1"}, "title, body ,action1"},
      {u"title",
       u" body ",
       {u"action1", u"action2"},
       "title, body ,action1,action2"},
  };

  for (size_t i = 0; i < tests.size(); ++i) {
    auto test = tests[i];
    blink::PlatformNotificationData notification_data;
    notification_data.title = test.title;
    notification_data.body = test.body;
    for (const auto& action_title : test.action_titles) {
      auto action = blink::mojom::NotificationAction::New();
      action->title = action_title;
      notification_data.actions.push_back(std::move(action));
    }
    notification_content_detection_model()->Execute(
        notification_data, GURL("url"), /*did_match_allowlist=*/false);
    histogram_tester().ExpectUniqueSample(
        kSuspiciousScoreHistogram, 100 * kSuspiciousScoreTestValue, 1 + i);
    EXPECT_EQ(notification_content_detection_model()->inputs()[i],
              test.expected_input);
  }
}

TEST_F(NotificationContentDetectionModelTest,
       LogNotificationSuspiciousScoreWithEmptyNotificationData) {
  // Update with a notification content detection model.
  SendModelToNotificationContentDetectionModel();

  blink::PlatformNotificationData notification_data;
  notification_content_detection_model()->Execute(
      notification_data, GURL("url"), /*did_match_allowlist=*/false);
  histogram_tester().ExpectUniqueSample(kSuspiciousScoreHistogram,
                                        100 * kSuspiciousScoreTestValue, 1);
  EXPECT_EQ(notification_content_detection_model()->inputs()[0], ",,");
}

}  // namespace safe_browsing
