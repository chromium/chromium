// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_

#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notification_content_detection_model.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

namespace blink {
struct PlatformNotificationData;
}  // namespace blink

namespace content {
class BrowserContext;
}  // namespace content

namespace safe_browsing {

class NotificationContentDetectionService : public KeyedService {
 public:
  NotificationContentDetectionService(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      content::BrowserContext* browser_context);
  ~NotificationContentDetectionService() override;

  // This method is virtual for testing.
  virtual void MaybeCheckNotificationContentDetectionModel(
      const blink::PlatformNotificationData& notification_data,
      const GURL& origin);

 protected:
  friend class NotificationContentDetectionServiceTest;
  void SetModelForTesting(std::unique_ptr<NotificationContentDetectionModel>
                              test_notification_content_detection_model);

 private:
  void OnCheckUrlForHighConfidenceAllowlist(
      blink::PlatformNotificationData& notification_data,
      const base::TimeTicks start_time,
      const GURL& origin,
      bool did_match_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<NotificationContentDetectionModel>
      notification_content_detection_model_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NotificationContentDetectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_NOTIFICATION_CONTENT_DETECTION_NOTIFICATION_CONTENT_DETECTION_SERVICE_H_
