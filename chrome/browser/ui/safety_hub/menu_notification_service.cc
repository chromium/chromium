// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace {
SafetyHubModuleInfoElement::SafetyHubModuleInfoElement() = default;
SafetyHubModuleInfoElement::~SafetyHubModuleInfoElement() = default;

SafetyHubModuleInfoElement::SafetyHubModuleInfoElement(
    MenuNotificationPriority priority,
    base::TimeDelta interval,
    base::RepeatingCallback<
        std::optional<std::unique_ptr<SafetyHubService::Result>>()>
        result_getter,
    std::unique_ptr<SafetyHubMenuNotification> notification)
    : priority(priority),
      interval(interval),
      result_getter(result_getter),
      notification(std::move(notification)) {}
}  // namespace

SafetyHubMenuNotificationService::SafetyHubMenuNotificationService(
    PrefService* pref_service,
    UnusedSitePermissionsService* unused_site_permissions_service,
    NotificationPermissionsReviewService* notification_permissions_service,
    extensions::CWSInfoService* extension_info_service,
    PasswordStatusCheckService* password_check_service,
    Profile* profile) {
  pref_service_ = std::move(pref_service);
  const base::Value::Dict& stored_notifications =
      pref_service_->GetDict(safety_hub_prefs::kMenuNotificationsPrefsKey);

  // TODO(crbug.com/1443466): Make the interval for each service finch
  // configurable.
  // The Safety Hub services will be available whenever the |GetCachedResult|
  // method is called, so it is safe to use |base::Unretained| here.
  SetInfoElement(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS,
      MenuNotificationPriority::LOW, base::Days(10),
      base::BindRepeating(&SafetyHubService::GetCachedResult,
                          base::Unretained(unused_site_permissions_service)),
      stored_notifications);
  SetInfoElement(
      safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS,
      MenuNotificationPriority::LOW, base::Days(10),
      base::BindRepeating(&SafetyHubService::GetCachedResult,
                          base::Unretained(notification_permissions_service)),
      stored_notifications);
  SetInfoElement(safety_hub::SafetyHubModuleType::SAFE_BROWSING,
                 MenuNotificationPriority::MEDIUM, base::Days(90),
                 base::BindRepeating(&SafetyHubSafeBrowsingResult::GetResult,
                                     base::Unretained(pref_service)),
                 stored_notifications);
  SetInfoElement(safety_hub::SafetyHubModuleType::EXTENSIONS,
                 MenuNotificationPriority::LOW, base::Days(10),
                 base::BindRepeating(&SafetyHubExtensionsResult::GetResult,
                                     base::Unretained(extension_info_service),
                                     profile, true),
                 stored_notifications);
  SetInfoElement(
      safety_hub::SafetyHubModuleType::PASSWORDS,
      MenuNotificationPriority::HIGH, base::Days(0),
      base::BindRepeating(&PasswordStatusCheckService::GetCachedResult,
                          base::Unretained(password_check_service)),
      stored_notifications);

  // Listen for changes to the Safe Browsing pref to accommodate the trigger
  // logic.
  registrar_.Init(pref_service);
  registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &SafetyHubMenuNotificationService::OnSafeBrowsingPrefUpdate,
          base::Unretained(this)));
}

SafetyHubMenuNotificationService::~SafetyHubMenuNotificationService() {
  registrar_.RemoveAll();
}

absl::optional<MenuNotificationEntry>
SafetyHubMenuNotificationService::GetNotificationToShow() {
  absl::optional<ResultMap> result_map = GetResultsFromAllModules();
  if (!result_map.has_value()) {
    return absl::nullopt;
  }
  std::list<SafetyHubMenuNotification*> notifications_to_be_shown;
  MenuNotificationPriority cur_highest_priority = MenuNotificationPriority::LOW;
  for (auto const& item : result_map.value()) {
    SafetyHubModuleInfoElement* info_element =
        module_info_map_[item.first].get();
    SafetyHubMenuNotification* notification = info_element->notification.get();
    notification->UpdateResult(std::move(result_map.value()[item.first]));
    int max_all_time_impressions =
        item.first == safety_hub::SafetyHubModuleType::SAFE_BROWSING ? 3 : 0;
    if (notification->ShouldBeShown(info_element->interval,
                                    max_all_time_impressions)) {
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
SafetyHubMenuNotificationService::GetResultsFromAllModules() {
  ResultMap result_map;
  for (auto const& item : module_info_map_) {
    CHECK(item.second->result_getter);
    absl::optional<std::unique_ptr<SafetyHubService::Result>> result =
        item.second->result_getter.Run();
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
    SafetyHubModuleInfoElement* info_element =
        module_info_map_.find(it.first)->second.get();
    notifications.Set(it.second, info_element->notification->ToDictValue());
  }
  pref_service_->SetDict(safety_hub_prefs::kMenuNotificationsPrefsKey,
                         std::move(notifications));
}

SafetyHubMenuNotification*
SafetyHubMenuNotificationService::GetNotificationForTesting(
    safety_hub::SafetyHubModuleType service_type) {
  return module_info_map_.find(service_type)->second.get()->notification.get();
}

std::unique_ptr<SafetyHubMenuNotification>
SafetyHubMenuNotificationService::GetNotificationFromDict(
    const base::Value::Dict& dict,
    safety_hub::SafetyHubModuleType& type) const {
  // It can be assumed that all `safety_hub::SafetyHubModuleType`s are in
  // `pref_dict_key_map_`.
  const base::Value::Dict* notification_dict =
      dict.FindDict(pref_dict_key_map_.find(type)->second);
  if (!notification_dict) {
    auto new_notification = std::make_unique<SafetyHubMenuNotification>(type);
    if (type == safety_hub::SafetyHubModuleType::SAFE_BROWSING) {
      new_notification->SetOnlyShowAfter(base::Time::Now() + base::Days(1));
    }
    return new_notification;
  }
  return std::make_unique<SafetyHubMenuNotification>(*notification_dict, type);
}

void SafetyHubMenuNotificationService::SetInfoElement(
    safety_hub::SafetyHubModuleType type,
    MenuNotificationPriority priority,
    base::TimeDelta interval,
    base::RepeatingCallback<
        std::optional<std::unique_ptr<SafetyHubService::Result>>()>
        result_getter,
    const base::Value::Dict& stored_notifications) {
  module_info_map_[type] = std::make_unique<SafetyHubModuleInfoElement>(
      priority, interval, result_getter,
      GetNotificationFromDict(stored_notifications, type));
}

void SafetyHubMenuNotificationService::OnSafeBrowsingPrefUpdate() {
  module_info_map_[safety_hub::SafetyHubModuleType::SAFE_BROWSING]
      ->notification->SetOnlyShowAfter(base::Time::Now() + base::Days(1));
  module_info_map_[safety_hub::SafetyHubModuleType::SAFE_BROWSING]
      ->notification->ResetAllTimeNotificationCount();
  SaveNotificationsToPrefs();
}

void SafetyHubMenuNotificationService::DismissActiveNotification() {
  for (auto const& item : module_info_map_) {
    if (item.second->notification->IsCurrentlyActive()) {
      item.second->notification->Dismiss();
    }
  }
}

void SafetyHubMenuNotificationService::DismissPasswordNotification() {
  // TODO(crbug.com/1443466): Uncomment the following lines in
  // crrev.com/c/4982626.
  // SafetyHubMenuNotification* notification =
  //     module_info_map_.at(safety_hub::SafetyHubModuleType::PASSWORDS)
  //         ->notification.get();
  // if (notification->IsCurrentlyActive()) {
  //   notification->Dismiss();
  // }
}

absl::optional<safety_hub::SafetyHubModuleType>
SafetyHubMenuNotificationService::GetModuleOfActiveNotification() const {
  for (auto const& item : module_info_map_) {
    if (item.second->notification->IsCurrentlyActive()) {
      return item.second->notification->GetModuleType();
    }
  }
  return absl::nullopt;
}
