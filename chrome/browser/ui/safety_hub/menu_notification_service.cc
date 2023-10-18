// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "components/prefs/pref_service.h"

namespace {
SafetyHubServiceInfoElement::SafetyHubServiceInfoElement() = default;
SafetyHubServiceInfoElement::~SafetyHubServiceInfoElement() = default;

SafetyHubServiceInfoElement::SafetyHubServiceInfoElement(
    MenuNotificationPriority priority,
    base::TimeDelta interval,
    raw_ptr<SafetyHubService> service,
    std::unique_ptr<SafetyHubMenuNotification> notification)
    : priority(priority),
      interval(interval),
      service(service),
      notification(std::move(notification)) {}
}  // namespace

SafetyHubMenuNotificationService::SafetyHubMenuNotificationService(
    PrefService* pref_service,
    UnusedSitePermissionsService* unused_site_permissions_service,
    NotificationPermissionsReviewService* notification_permissions_service) {
  pref_service_ = std::move(pref_service);
  const base::Value::Dict& stored_notifications =
      pref_service_->GetDict(safety_hub_prefs::kMenuNotificationsPrefsKey);

  // TODO(crbug.com/1443466): Make the interval for each service finch
  // configurable.
  SetServiceInfoElement(SafetyHubServiceType::UNUSED_SITE_PERMISSIONS,
                        MenuNotificationPriority::LOW, base::Days(10),
                        unused_site_permissions_service, stored_notifications);
  SetServiceInfoElement(SafetyHubServiceType::NOTIFICATION_PERMISSIONS,
                        MenuNotificationPriority::LOW, base::Days(10),
                        notification_permissions_service, stored_notifications);
}

SafetyHubMenuNotificationService::~SafetyHubMenuNotificationService() = default;

absl::optional<MenuNotificationEntry>
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
    // The notifications should be persisted with updated results.
    SaveNotificationsToPrefs();
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
  // The information related to showing the notification needs to be persisted
  // as well.
  SaveNotificationsToPrefs();
  return MenuNotificationEntry(notification_to_show->GetNotificationCommandId(),
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

void SafetyHubMenuNotificationService::SaveNotificationsToPrefs() const {
  base::Value::Dict notifications;
  for (auto const& it : pref_dict_key_map_) {
    SafetyHubServiceInfoElement* info_element =
        service_info_map_.find(it.first)->second.get();
    notifications.Set(it.second, info_element->notification->ToDictValue());
  }
  pref_service_->SetDict(safety_hub_prefs::kMenuNotificationsPrefsKey,
                         std::move(notifications));
}

SafetyHubMenuNotification*
SafetyHubMenuNotificationService::GetNotificationForTesting(
    SafetyHubServiceType service_type) {
  return service_info_map_.find(service_type)->second.get()->notification.get();
}

std::unique_ptr<SafetyHubMenuNotification>
SafetyHubMenuNotificationService::GetNotificationFromDict(
    const base::Value::Dict& dict,
    SafetyHubServiceType type,
    SafetyHubService* service) const {
  // It can be assumed that all `SafetyHubServiceType`s are in
  // `pref_dict_key_map_`.
  const base::Value::Dict* notification_dict =
      dict.FindDict(pref_dict_key_map_.find(type)->second);
  std::unique_ptr<SafetyHubMenuNotification> result_notification;
  if (!notification_dict) {
    result_notification = std::make_unique<SafetyHubMenuNotification>();
  } else {
    result_notification =
        SafetyHubMenuNotification::FromDictValue(*notification_dict, service);
  }
  return result_notification;
}

void SafetyHubMenuNotificationService::Shutdown() {
  for (auto const& item : service_info_map_) {
    // Setting to nullptr to avoid dangling pointers when services are
    // deconstructed.
    item.second->service = nullptr;
  }
}

void SafetyHubMenuNotificationService::SetServiceInfoElement(
    SafetyHubServiceType type,
    MenuNotificationPriority priority,
    base::TimeDelta interval,
    SafetyHubService* service,
    const base::Value::Dict& stored_notifications) {
  service_info_map_[type] = std::make_unique<SafetyHubServiceInfoElement>(
      priority, interval, service,
      GetNotificationFromDict(stored_notifications, type, service));
}
