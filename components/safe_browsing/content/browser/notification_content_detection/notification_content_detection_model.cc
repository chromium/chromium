// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"

namespace safe_browsing {

NotificationContentDetectionModel::NotificationContentDetectionModel(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : BertModelHandler(model_provider,
                       background_task_runner,
                       optimization_guide::proto::
                           OPTIMIZATION_TARGET_NOTIFICATION_CONTENT_DETECTION,
                       std::nullopt) {}

NotificationContentDetectionModel::~NotificationContentDetectionModel() =
    default;

void NotificationContentDetectionModel::Execute(
    const std::u16string& contents) {
  // If there is no model version, then there is no valid notification content
  // detection model loaded from the server so don't check the model.
  if (!GetModelInfo() || !GetModelInfo()->GetVersion()) {
    return;
  }
  // Invoke parent to execute the notification content detection tflite model
  // with `contents` as input.
  ExecuteModelWithInput(
      base::BindOnce(&NotificationContentDetectionModel::PostprocessCategories,
                     weak_ptr_factory_.GetWeakPtr()),
      base::UTF16ToUTF8(contents));
}

void NotificationContentDetectionModel::PostprocessCategories(
    const std::optional<std::vector<tflite::task::core::Category>>& output) {
  // Validate model response and obtain suspicious and not suspicious confidence
  // scores. Crash on debug builds only.
  DCHECK(output);
  DCHECK_EQ(output->size(), 2UL);
  for (const auto& category : *output) {
    if (category.class_name == kSuspiciousVerdictLabel) {
      // Log "suspicious" score from model's response.
      base::UmaHistogramPercentage(kSuspiciousScoreHistogram,
                                   100 * category.score);
      return;
    }
  }
  // Enforce this crash on debug builds only.
  DCHECK(false) << "Could not find the right class name in the model response";
}

}  // namespace safe_browsing
