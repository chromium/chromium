// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_

#include <list>
#include <map>
#include <memory>

#include "base/time/time.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

enum SafetyHubServiceType {
  UNUSED_SITE_PERMISSIONS,
};

enum MenuNotificationPriority {
  LOW = 0,
  MEDIUM,
  HIGH,
};

struct SafetyHubServiceInfoElement {
  SafetyHubServiceInfoElement();
  ~SafetyHubServiceInfoElement();
  SafetyHubServiceInfoElement(
      const char* name,
      MenuNotificationPriority priority,
      base::TimeDelta interval,
      raw_ptr<SafetyHubService> service,
      std::unique_ptr<SafetyHubMenuNotification> notification);

  const char* name;
  MenuNotificationPriority priority;
  base::TimeDelta interval;
  raw_ptr<SafetyHubService> service;
  std::unique_ptr<SafetyHubMenuNotification> notification;
};

using ResultMap =
    std::map<SafetyHubServiceType, std::unique_ptr<SafetyHubService::Result>>;

}  // namespace

// This class manages the notifications that should be shown when a user opens
// the three-dot menu. It will collect the latest results from all the Safety
// Hub service and subsequently update the notifications. Based on priority and
// prior showing of notification, it will determine which notification that
// should be shown.
class SafetyHubMenuNotificationService : public KeyedService {
 public:
  explicit SafetyHubMenuNotificationService(
      UnusedSitePermissionsService* unused_site_permissions_service);
  SafetyHubMenuNotificationService(const SafetyHubMenuNotificationService&) =
      delete;
  SafetyHubMenuNotificationService& operator=(
      const SafetyHubMenuNotificationService&) = delete;

  ~SafetyHubMenuNotificationService() override;

  // Returns the CommandID and notification string that should be shown in the
  // three-dot menu. When no notification should be shown, absl::nullopt will be
  // returned.
  absl::optional<std::pair<int, std::u16string>> GetNotificationToShow();

 private:
  // Gets the latest result from each Safety Hub service. Will return
  // absl::nullopt when there is no result from one of the services.
  absl::optional<ResultMap> GetResultsFromAllServices();

  std::map<SafetyHubServiceType, std::unique_ptr<SafetyHubServiceInfoElement>>
      service_info_map_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
