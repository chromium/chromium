// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_NOTIFICATION_CONTENT_DETECTION_MODEL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_NOTIFICATION_CONTENT_DETECTION_MODEL_H_

#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"

namespace safe_browsing {

class TestNotificationContentDetectionModel
    : public NotificationContentDetectionModel {
 public:
  TestNotificationContentDetectionModel(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      content::BrowserContext* browser_context);

  ~TestNotificationContentDetectionModel() override;

  void ExecuteModelWithInput(ExecutionCallback callback,
                             const std::string& input) override;

  const std::vector<std::string>& inputs() const { return inputs_; }

 private:
  std::vector<std::string> inputs_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_TEST_NOTIFICATION_CONTENT_DETECTION_MODEL_H_
