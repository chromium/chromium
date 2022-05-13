// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_H_
#define COMPONENTS_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace permissions {

// This class records and stores notification engagement per origin for the
// past 90 days. Engagements per origin are bucketed by week: A notification
// engagement or display is assigned to the last Monday midnight in local time.
class NotificationsEngagementService : public KeyedService {
 public:
  explicit NotificationsEngagementService(content::BrowserContext* context,
                                          PrefService* pref_service);

  NotificationsEngagementService(const NotificationsEngagementService&) =
      delete;
  NotificationsEngagementService& operator=(
      const NotificationsEngagementService&) = delete;

  ~NotificationsEngagementService() override = default;

  // KeyedService implementation.
  void Shutdown() override;

  void RecordNotificationDisplayed(const GURL& url);
  void RecordNotificationInteraction(const GURL& url);

  // ISO8601 defines Monday as the first day of the week. Additionally, in most
  // of the world the workweek starts with Monday, so Monday is used
  // specifically here, as notification usage is a function of work/personal
  // settings.
  static std::string GetBucketLabelForLastMonday(base::Time time);
  static absl::optional<base::Time> ParsePeriodBeginFromBucketLabel(
      const std::string& label);

 private:
  void IncrementCounts(const GURL& url,
                       const int display_count_delta,
                       const int click_count_delta);

  // Used to update the notification engagement per URL.
  raw_ptr<HostContentSettingsMap> settings_map_;

  raw_ptr<PrefService> pref_service_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_NOTIFICATIONS_ENGAGEMENT_SERVICE_H_
