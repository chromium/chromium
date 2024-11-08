// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/test_notification_content_detection_model.h"

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"

namespace safe_browsing {

namespace {

constexpr double kSuspiciousScoreTestValue = 0.59;
constexpr double kNotSuspiciousScoreTestValue = 0.41;

}  // namespace

TestNotificationContentDetectionModel::TestNotificationContentDetectionModel(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    content::BrowserContext* browser_context)
    : NotificationContentDetectionModel(model_provider,
                                        background_task_runner,
                                        browser_context) {}
TestNotificationContentDetectionModel::
    ~TestNotificationContentDetectionModel() = default;

void TestNotificationContentDetectionModel::ExecuteModelWithInput(
    ExecutionCallback callback,
    const std::string& input) {
  inputs_.push_back(input);
  std::vector<tflite::task::core::Category> model_output = {
      {"suspicious", kSuspiciousScoreTestValue},
      {"not suspicious", kNotSuspiciousScoreTestValue}};
  std::move(callback).Run(model_output);
}

}  // namespace safe_browsing
