// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_

#include <list>
#include <map>
#include <memory>

#include "base/time/time.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct MenuNotificationEntry {
  int command = 0;
  std::u16string label;
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
      extensions::CWSInfoService* extension_info_service,
      PasswordStatusCheckService* password_check_service,
      Profile* profile);
  SafetyHubMenuNotificationService(const SafetyHubMenuNotificationService&) =
      delete;
  SafetyHubMenuNotificationService& operator=(
      const SafetyHubMenuNotificationService&) = delete;

  ~SafetyHubMenuNotificationService() override;

  // Returns the CommandID and notification string that should be shown in the
  // three-dot menu. When no notification should be shown, absl::nullopt will be
  // returned.
  absl::optional<MenuNotificationEntry> GetNotificationToShow();

  // Dismisses all the active menu notifications.
  void DismissActiveNotification();

  // Dismisses the active menu notification of the password module.
  void DismissPasswordNotification();

  // Returns the module of the notification that is currently active.
  absl::optional<safety_hub::SafetyHubModuleType>
  GetModuleOfActiveNotification() const;

  // Returns the |service_info_map_|. For testing purposes only.
  SafetyHubMenuNotification* GetNotificationForTesting(
      safety_hub::SafetyHubModuleType service_type);

 private:
  // Gets the latest result from each Safety Hub service. Will return
  // absl::nullopt when there is no result from one of the services.
  absl::optional<ResultMap> GetResultsFromAllModules();

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

  const std::map<safety_hub::SafetyHubModuleType, const char*>
      pref_dict_key_map_ = {
          {safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS,
           "unused-site-permissions"},
          {safety_hub::SafetyHubModuleType::NOTIFICATION_PERMISSIONS,
           "notification-permissions"},
          {safety_hub::SafetyHubModuleType::SAFE_BROWSING, "safe-browsing"},
          {safety_hub::SafetyHubModuleType::EXTENSIONS, "extensions"},
          {safety_hub::SafetyHubModuleType::PASSWORDS, "passwords"},
      };

  // Preference service that persists the notifications.
  raw_ptr<PrefService> pref_service_;

  // A map that captures the meta information about menu notifications for each
  // Safety Hub module.
  std::map<safety_hub::SafetyHubModuleType,
           std::unique_ptr<SafetyHubModuleInfoElement>>
      module_info_map_;

  // Registrar to record the pref changes to Safe Browsing.
  PrefChangeRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_H_
