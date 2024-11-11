// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}

namespace safe_browsing {

NotificationContentDetectionService::NotificationContentDetectionService(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    content::BrowserContext* browser_context) {
  database_manager_ = database_manager;
  notification_content_detection_model_ =
      std::make_unique<NotificationContentDetectionModel>(
          model_provider, background_task_runner, browser_context);
}

NotificationContentDetectionService::~NotificationContentDetectionService() =
    default;

void NotificationContentDetectionService::
    MaybeCheckNotificationContentDetectionModel(
        const blink::PlatformNotificationData& notification_data,
        const GURL& origin) {
  // Check the high confidence allowlist to determine whether to check the
  // LiteRT model. Since this does not own `notification_data`, create a deep
  // copy and pass it along so that the value is safe to change.
  blink::PlatformNotificationData notification_data_copy = notification_data;
  database_manager_->CheckUrlForHighConfidenceAllowlist(
      origin, base::BindOnce(&NotificationContentDetectionService::
                                 OnCheckUrlForHighConfidenceAllowlist,
                             weak_factory_.GetWeakPtr(),
                             base::OwnedRef(notification_data_copy),
                             base::TimeTicks::Now(), origin));
}

void NotificationContentDetectionService::SetModelForTesting(
    std::unique_ptr<NotificationContentDetectionModel>
        test_notification_content_detection_model) {
  notification_content_detection_model_ =
      std::move(test_notification_content_detection_model);
}

void NotificationContentDetectionService::OnCheckUrlForHighConfidenceAllowlist(
    blink::PlatformNotificationData& notification_data,
    const base::TimeTicks start_time,
    const GURL& origin,
    bool did_match_allowlist,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  base::UmaHistogramTimes(kAllowlistCheckLatencyHistogram,
                          base::TimeTicks::Now() - start_time);
  // Only perform inference on the model for non-allowlisted URLs.
  if (did_match_allowlist) {
    // The model check should happen at a sampled rate for notifications from
    // allowlisted sites. This rate is defined by the
    // `kOnDeviceNotificationContentDetectionModelAllowlistSamplingRate` feature
    // parameter.
    if (base::RandDouble() * 100 >=
        kOnDeviceNotificationContentDetectionModelAllowlistSamplingRate.Get()) {
      return;
    }
  }

  // Perform inference with model on `notification_contents`.
  notification_content_detection_model_->Execute(notification_data, origin,
                                                 did_match_allowlist);
}

}  // namespace safe_browsing
