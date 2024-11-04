// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_MODEL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_MODEL_H_

#include <string>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/bert_model_handler.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"
#include "url/gurl.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace safe_browsing {

// This class handles all interactions with the notification content detection
// model, including fetching the latest model version from the server,
// performing inference on it, and handling the model's output. The model fetch
// is requested in the constructor and delivered within a couple of minutes.

// Since the notification content detection model is based on a BERT model, this
// class leverages `optimization_guide::BertModelHandler` for handling model
// operations and thread synchronization. This must be run on the UI thread.
class NotificationContentDetectionModel
    : public optimization_guide::BertModelHandler {
 public:
  NotificationContentDetectionModel(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~NotificationContentDetectionModel() override;

  // Perform inference on the model with the provided notification contents.
  // Pass `PostprocessCategories` as the `ExecuteModelWithInput` callback. This
  // method is virtual for testing.
  virtual void Execute(blink::PlatformNotificationData& notification_data);

 private:
  // Log UMA metrics, given the `output` result of model inference.
  void PostprocessCategories(
      const std::optional<std::vector<tflite::task::core::Category>>& output);

  base::WeakPtrFactory<NotificationContentDetectionModel> weak_ptr_factory_{
      this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_MODEL_H_
