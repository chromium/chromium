// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <memory>
#include <utility>

namespace {
SafetyHubServiceInfoElement::SafetyHubServiceInfoElement() = default;
SafetyHubServiceInfoElement::~SafetyHubServiceInfoElement() = default;

SafetyHubServiceInfoElement::SafetyHubServiceInfoElement(
    const char* name,
    MenuNotificationPriority priority,
    base::TimeDelta interval,
    raw_ptr<SafetyHubService> service,
    std::unique_ptr<SafetyHubMenuNotification> notification)
    : name(name),
      priority(priority),
      interval(interval),
      service(service),
      notification(std::move(notification)) {}
}  // namespace

SafetyHubMenuNotificationService::SafetyHubMenuNotificationService(
    UnusedSitePermissionsService* unused_site_permissions_service) {
  // TODO(crbug.com/1443466): Read the notifications from disk.
  // TODO(crbug.com/1443466): Make the interval for each service finch
  // configurable.
  service_info_map_[SafetyHubServiceType::UNUSED_SITE_PERMISSIONS] =
      std::make_unique<SafetyHubServiceInfoElement>(
          "unused-permissions", MenuNotificationPriority::LOW, base::Days(10),
          unused_site_permissions_service,
          std::make_unique<SafetyHubMenuNotification>());
}

SafetyHubMenuNotificationService::~SafetyHubMenuNotificationService() = default;

absl::optional<std::pair<int, std::u16string>>
SafetyHubMenuNotificationService::GetNotificationToShow() {
  absl::optional<ResultMap> result_map = GetResultsFromAllServices();
  if (!result_map.has_value()) {
    return absl::nullopt;
  }
  std::list<SafetyHubMenuNotification*> notifications_to_be_shown;
  MenuNotificationPriority cur_highest_priority = MenuNotificationPriority::LOW;
  for (auto const& item : result_map.value()) {
    SafetyHubServiceInfoElement* info_element =
        service_info_map_[item.first].get();
    SafetyHubMenuNotification* notification = info_element->notification.get();
    notification->UpdateResult(std::move(result_map.value()[item.first]));
    if (notification->ShouldBeShown(info_element->interval)) {
      // Notifications are first sorted by priority, and then by being currently
      // active.
      if (info_element->priority > cur_highest_priority ||
          (info_element->priority == cur_highest_priority &&
           notification->IsCurrentlyActive())) {
        cur_highest_priority = info_element->priority;
        notifications_to_be_shown.push_front(notification);
      } else {
        notifications_to_be_shown.push_back(notification);
      }
    } else {
      if (notification->IsCurrentlyActive()) {
        notification->Dismiss();
      }
    }
  }
  if (notifications_to_be_shown.empty()) {
    return absl::nullopt;
  }
  SafetyHubMenuNotification* notification_to_show =
      notifications_to_be_shown.front();
  // Dismiss all other notifications that are not shown.
  for (auto it = std::next(notifications_to_be_shown.begin());
       it != notifications_to_be_shown.end(); ++it) {
    (*it)->Dismiss();
  }
  notification_to_show->Show();
  return std::make_pair(notification_to_show->GetNotificationCommandId(),
                        notification_to_show->GetNotificationString());
}

absl::optional<ResultMap>
SafetyHubMenuNotificationService::GetResultsFromAllServices() {
  ResultMap result_map;
  for (auto const& item : service_info_map_) {
    auto result = item.second->service->GetCachedResult();
    // If one of the cached results is unavailable, no notification is shown.
    if (!result.has_value()) {
      return absl::nullopt;
    }
    result_map[item.first] = std::move(result.value());
  }
  return result_map;
}
