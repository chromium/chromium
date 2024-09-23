// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_

#include <list>
#include <map>
#include <memory>
#include <optional>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/keyed_service/core/keyed_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#endif  // BUILDFLAG(IS_ANDROID)

struct MenuNotificationEntry {
  int command = 0;
  std::u16string label;
  safety_hub::SafetyHubModuleType module;
};

namespace {

enum MenuNotificationPriority {
  LOW = 0,
  MEDIUM,
  HIGH,
};

struct SafetyHubModuleInfoElement {
  SafetyHubModuleInfoElement();
  ~SafetyHubModuleInfoElement();
  SafetyHubModuleInfoElement(
      MenuNotificationPriority priority,
      base::TimeDelta interval,
      base::RepeatingCallback<
          std::optional<std::unique_ptr<SafetyHubService::Result>>()>
          result_getter,
      std::unique_ptr<SafetyHubMenuNotification> notification);

  MenuNotificationPriority priority;
  base::TimeDelta interval;
  base::RepeatingCallback<
      std::optional<std::unique_ptr<SafetyHubService::Result>>()>
      result_getter;
  std::unique_ptr<SafetyHubMenuNotification> notification;
};

using ResultMap = std::map<safety_hub::SafetyHubModuleType,
                           std::unique_ptr<SafetyHubService::Result>>;

}  // namespace

// This class manages the notifications that should be shown when a user opens
// the three-dot menu. It will collect the latest results from all the Safety
// Hub service and subsequently update the notifications. Based on priority and
// prior showing of notification, it will determine which notification that
// should be shown.
class SafetyHubMenuNotificationService : public KeyedService {
 public:
  explicit SafetyHubMenuNotificationService(
      PrefService* pref_service,
      UnusedSitePermissionsService* unused_site_permissions_service,
      NotificationPermissionsReviewService* notification_permissions_service,
#if !BUILDFLAG(IS_ANDROID)
      PasswordStatusCheckService* password_check_service,
#endif  // BUILDFLAG(IS_ANDROID)
      Profile* profile);
  SafetyHubMenuNotificationService(const SafetyHubMenuNotificationService&) =
      delete;
  SafetyHubMenuNotificationService& operator=(
      const SafetyHubMenuNotificationService&) = delete;

  ~SafetyHubMenuNotificationService() override;

  // Returns the CommandID and notification string that should be shown in the
  // three-dot menu. When no notification should be shown, std::nullopt will be
  // returned.
  std::optional<MenuNotificationEntry> GetNotificationToShow();

  // Dismisses all the active menu notifications.
  void DismissActiveNotification();

  // Dismisses the active menu notification of the specified module.
  void DismissActiveNotificationOfModule(
      safety_hub::SafetyHubModuleType module);

  // Returns the module of the notification that was last displayed to the user.
  std::optional<safety_hub::SafetyHubModuleType>
  GetLastShownNotificationModule() const;

  void UpdateResultGetterForTesting(
      safety_hub::SafetyHubModuleType type,
      base::RepeatingCallback<
          std::optional<std::unique_ptr<SafetyHubService::Result>>()>
          result_getter);

 private:
  // Gets the latest result from each Safety Hub service. Will return
  // std::nullopt when there is no result from one of the services.
  std::optional<ResultMap> GetResultsFromAllModules();

  // Stores the notifications (which should have their results updated) as a
  // dict in the prefs.
  void SaveNotificationsToPrefs() const;

  // Creates a notification from the provided dictionary, for the specified
  // Safety Hub service type.
  std::unique_ptr<SafetyHubMenuNotification> GetNotificationFromDict(
      const base::Value::Dict& dict,
      safety_hub::SafetyHubModuleType& type) const;

  // Sets the relevant, static meta information for the three-dot menu
  // (priority, interval, and method to retrieve the relevant result) for a
  // specific type of Safety Hub module provided the dictionary that stores the
  // notifications.
  void SetInfoElement(
      safety_hub::SafetyHubModuleType type,
      MenuNotificationPriority priority,
      base::TimeDelta interval,
      base::RepeatingCallback<
          std::optional<std::unique_ptr<SafetyHubService::Result>>()>
          result_getter,
      const base::Value::Dict& stored_notifications);

  // Called when the pref for Safe Browsing has been updated.
  void OnSafeBrowsingPrefUpdate();

  // Returns if any safety hub notification is shown in the menu so far.
  bool HasAnyNotificationBeenShown() const;

  // Holds the mapping from module type to pref name.
  std::map<safety_hub::SafetyHubModuleType, const char*> pref_dict_key_map_;

  // Preference service that persists the notifications.
  raw_ptr<PrefService> pref_service_;

  // A map that captures the meta information about menu notifications for each
  // Safety Hub module.
  std::map<safety_hub::SafetyHubModuleType,
           std::unique_ptr<SafetyHubModuleInfoElement>>
      module_info_map_;

  // Registrar to record the pref changes to Safe Browsing.
  PrefChangeRegistrar registrar_;

  // The module of the last notification that has been shown.
  std::optional<safety_hub::SafetyHubModuleType> last_shown_module_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
