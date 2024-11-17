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
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/password_status_check_result_android.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#endif  // BUILDFLAG(IS_ANDROID)
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
#if !BUILDFLAG(IS_ANDROID)
    PasswordStatusCheckService* password_check_service,
#endif  // !BUILDFLAG(IS_ANDROID)
    Profile* profile) {
  pref_service_ = std::move(pref_service);
  const base::Value::Dict& stored_notifications =
      pref_service_->GetDict(safety_hub_prefs::kMenuNotificationsPrefsKey);

  pref_dict_key_map_ = {
      {safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS,
       "unused-site-permissions"},
      {safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS,
       "notification-permissions"},
      {safety_hub::SafetyHubModuleType::SAFE_BROWSING, "safe-browsing"},
  };

  // TODO(crbug.com/40267370): Make the interval for each service finch
  // configurable.
  // The Safety Hub services will be available whenever the |GetCachedResult|
  // method is called, so it is safe to use |base::Unretained| here.
  SetInfoElement(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS,
      MenuNotificationPriority::LOW,
      features::kRevokedPermissionsNotificationInterval.Get(),
      base::BindRepeating(&SafetyHubService::GetCachedResult,
                          base::Unretained(unused_site_permissions_service)),
      stored_notifications);
  SetInfoElement(
      safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS,
      MenuNotificationPriority::LOW,
      features::kNotificationPermissionsNotificationInterval.Get(),
      base::BindRepeating(&SafetyHubService::GetCachedResult,
                          base::Unretained(notification_permissions_service)),
      stored_notifications);
  SetInfoElement(safety_hub::SafetyHubModuleType::SAFE_BROWSING,
                 MenuNotificationPriority::MEDIUM,
                 features::kSafeBrowsingNotificationInterval.Get(),
                 base::BindRepeating(&SafetyHubSafeBrowsingResult::GetResult,
                                     base::Unretained(pref_service)),
                 stored_notifications);

// Extensions are not available on Android, so we cannot fetch any information
// about them. Passwords are handled by GMS Core on Android and our
// PasswordStatusCheckService is not compatible with GMS Core.
#if !BUILDFLAG(IS_ANDROID)
  pref_dict_key_map_.emplace(safety_hub::SafetyHubModuleType::EXTENSIONS,
                             "extensions");
  SetInfoElement(safety_hub::SafetyHubModuleType::EXTENSIONS,
                 MenuNotificationPriority::LOW, base::Days(10),
                 base::BindRepeating(&SafetyHubExtensionsResult::GetResult,
                                     profile, true),
                 stored_notifications);

  // PasswordStatusCheckService might be null for some profiles and testing. Add
  // the info item only if the service is available.
  if (password_check_service) {
    pref_dict_key_map_.emplace(safety_hub::SafetyHubModuleType::PASSWORDS,
                               "passwords");
    SetInfoElement(
        safety_hub::SafetyHubModuleType::PASSWORDS,
        MenuNotificationPriority::HIGH,
        features::kPasswordCheckNotificationInterval.Get(),
        base::BindRepeating(&PasswordStatusCheckService::GetCachedResult,
                            base::Unretained(password_check_service)),
        stored_notifications);
  }
#else   // !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSafetyHub) &&
      base::FeatureList::IsEnabled(features::kSafetyHubFollowup)) {
    pref_dict_key_map_.emplace(safety_hub::SafetyHubModuleType::PASSWORDS,
                               "passwords");
    SetInfoElement(
        safety_hub::SafetyHubModuleType::PASSWORDS,
        MenuNotificationPriority::HIGH,
        features::kPasswordCheckNotificationInterval.Get(),
        base::BindRepeating(&PasswordStatusCheckResultAndroid::GetResult,
                            base::Unretained(pref_service)),
        stored_notifications);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Listen for changes to the Safe Browsing pref to accommodate the trigger
  // logic.
  registrar_.Init(pref_service);
  registrar_.Add(
      prefs::kSafeBrowsingEnabled,
      base::BindRepeating(
          &SafetyHubMenuNotificationService::OnSafeBrowsingPrefUpdate,
          base::Unretained(this)));

#if !BUILDFLAG(IS_ANDROID)
  // If any notification is not shown yet, trigger Hats survey control group.
  if (base::FeatureList::IsEnabled(features::kSafetyHubHaTSOneOffSurvey) &&
      !HasAnyNotificationBeenShown()) {
    HatsService* hats_service = HatsServiceFactory::GetForProfile(
        profile, /*create_if_necessary=*/true);
    if (!hats_service) {
      return;
    }
    hats_service->LaunchSurvey(
        kHatsSurveyTriggerSafetyHubOneOffExperimentControl);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void SafetyHubMenuNotificationService::UpdateResultGetterForTesting(
    safety_hub::SafetyHubModuleType type,
    base::RepeatingCallback<
        std::optional<std::unique_ptr<SafetyHubService::Result>>()>
        result_getter) {
  module_info_map_[type]->result_getter = result_getter;
}

SafetyHubMenuNotificationService::~SafetyHubMenuNotificationService() {
  registrar_.RemoveAll();
}

std::optional<MenuNotificationEntry>
SafetyHubMenuNotificationService::GetNotificationToShow() {
  std::optional<ResultMap> result_map = GetResultsFromAllModules();
  if (!result_map.has_value()) {
    return std::nullopt;
  }
  std::list<SafetyHubMenuNotification*> notifications_to_be_shown;
  MenuNotificationPriority cur_highest_priority = MenuNotificationPriority::LOW;
  for (auto& item : result_map.value()) {
    const SafetyHubModuleInfoElement* info_element =
        module_info_map_[item.first].get();
    SafetyHubMenuNotification* notification = info_element->notification.get();
    // The result in the ResultMap (item.second) is being moved away from and
    // thus shouldn't be used again in this method.
    notification->UpdateResult(std::move(item.second));
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
    return std::nullopt;
  }
  SafetyHubMenuNotification* notification_to_show =
      notifications_to_be_shown.front();
  // Dismiss all other notifications that are not shown.
  for (auto it = std::next(notifications_to_be_shown.begin());
       it != notifications_to_be_shown.end(); ++it) {
    (*it)->Dismiss();
  }
  notification_to_show->Show();
  last_shown_module_ = notification_to_show->GetModuleType();

  // The information related to showing the notification needs to be persisted
  // as well.
  SaveNotificationsToPrefs();
  return MenuNotificationEntry(notification_to_show->GetNotificationCommandId(),
                               notification_to_show->GetNotificationString(),
                               notification_to_show->GetModuleType());
}

std::optional<ResultMap>
SafetyHubMenuNotificationService::GetResultsFromAllModules() {
  ResultMap result_map;
  for (auto const& item : module_info_map_) {
    CHECK(item.second->result_getter);
    std::optional<std::unique_ptr<SafetyHubService::Result>> result =
        item.second->result_getter.Run();
    // If one of the cached results is unavailable, no notification is shown.
    if (!result.has_value()) {
      return std::nullopt;
    }
    result_map.try_emplace(item.first, std::move(result.value()));
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

void SafetyHubMenuNotificationService::DismissActiveNotificationOfModule(
    safety_hub::SafetyHubModuleType module) {
  // Callers of this function do not know if the module is available. Do
  // nothing, if the module is not available.
  if (!module_info_map_.contains(module)) {
    return;
  }
  SafetyHubMenuNotification* notification =
      module_info_map_.at(module)->notification.get();
  if (notification->IsCurrentlyActive()) {
    notification->Dismiss();
  }
}

std::optional<safety_hub::SafetyHubModuleType>
SafetyHubMenuNotificationService::GetLastShownNotificationModule() const {
  return last_shown_module_;
}

bool SafetyHubMenuNotificationService::HasAnyNotificationBeenShown() const {
  for (auto const& it : pref_dict_key_map_) {
    SafetyHubModuleInfoElement* info_element =
        module_info_map_.find(it.first)->second.get();
    if (info_element->notification.get()->HasAnyNotificationBeenShown()) {
      return true;
    }
  }
  return false;
}
