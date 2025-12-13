// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_service.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_constants.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"
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
        const GURL& origin,
        bool is_allowlisted_by_user,
        ModelVerdictCallback model_verdict_callback) {
  // Check the high confidence allowlist to determine whether to check the
  // LiteRT model. Since this does not own `notification_data`, create a deep
  // copy and pass it along so that the value is safe to change.
  blink::PlatformNotificationData notification_data_copy = notification_data;
  if (base::FeatureList::IsEnabled(
          kGlobalCacheListForGatingNotificationProtections)) {
    OnCheckUrlForHighConfidenceAllowlist(
        notification_data_copy, base::TimeTicks::Now(), origin,
        is_allowlisted_by_user, std::move(model_verdict_callback),
        ShouldSkipNotificationProtectionsDueToGlobalCacheList(origin),
        std::nullopt);
  } else {
    database_manager_->CheckUrlForHighConfidenceAllowlist(
        origin,
        base::BindOnce(&NotificationContentDetectionService::
                           OnCheckUrlForHighConfidenceAllowlist,
                       weak_factory_.GetWeakPtr(),
                       base::OwnedRef(notification_data_copy),
                       base::TimeTicks::Now(), origin, is_allowlisted_by_user,
                       std::move(model_verdict_callback)));
  }
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
    bool is_allowlisted_by_user,
    ModelVerdictCallback model_verdict_callback,
    bool did_match_allowlist,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  base::UmaHistogramTimes(kAllowlistCheckLatencyHistogram,
                          base::TimeTicks::Now() - start_time);
  bool should_skip_notification_warning =
      did_match_allowlist || is_allowlisted_by_user;
  if (should_skip_notification_warning) {
    // If the `origin` is on the high confidence allowlist or was allowlisted by
    // the user, then display the notification before checking the model for
    // logging metrics.
    std::move(model_verdict_callback)
        .Run(/*is_suspicious=*/false,
             NotificationContentDetectionModel::GetSerializedMetadata(
                 did_match_allowlist, is_allowlisted_by_user, std::nullopt));
    // Perform inference with model on `notification_contents` for metrics.
    // Since `model_verdict_callback` was already run above, use `DoNothing()`
    // as the callback.
    notification_content_detection_model_->Execute(
        notification_data, origin, is_allowlisted_by_user, did_match_allowlist,
        base::DoNothing());
    return;
  }
  // Perform inference with model on `notification_contents`, passing
  // `model_verdict_callback` to be called when the model is finished running.
  notification_content_detection_model_->Execute(
      notification_data, origin, is_allowlisted_by_user, did_match_allowlist,
      std::move(model_verdict_callback));
}

}  // namespace safe_browsing
