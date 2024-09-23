// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_TETHER_NOTIFICATION_PRESENTER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_TETHER_NOTIFICATION_PRESENTER_H_

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/tether/notification_presenter.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;
class PrefRegistrySimple;

namespace ash {
class NetworkConnect;
}

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash::tether {

// Produces notifications associated with CrOS tether network events and alerts
// observers about interactions with those notifications.
class TetherNotificationPresenter : public NotificationPresenter {
 public:
  // Caller must ensure that |profile| and |network_connect| outlive this
  // instance.
  TetherNotificationPresenter(Profile* profile,
                              NetworkConnect* network_connect);

  TetherNotificationPresenter(const TetherNotificationPresenter&) = delete;
  TetherNotificationPresenter& operator=(const TetherNotificationPresenter&) =
      delete;

  ~TetherNotificationPresenter() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // NotificationPresenter:
  void NotifyPotentialHotspotNearby(const std::string& device_id,
                                    const std::string& device_name,
                                    int signal_strength) override;
  void NotifyMultiplePotentialHotspotsNearby() override;
  NotificationPresenter::PotentialHotspotNotificationState
  GetPotentialHotspotNotificationState() override;
  void RemovePotentialHotspotNotification() override;
  void NotifySetupRequired(const std::string& device_name,
                           int signal_strength) override;
  void RemoveSetupRequiredNotification() override;
  void NotifyConnectionToHostFailed() override;
  void RemoveConnectionToHostFailedNotification() override;

  class SettingsUiDelegate {
   public:
    virtual ~SettingsUiDelegate() {}

    // Displays the settings page (opening a new window if necessary) at the
    // provided subpage for the user with the Profile |profile|.
    virtual void ShowSettingsSubPageForProfile(Profile* profile,
                                               const std::string& sub_page) = 0;
  };

 private:
  friend class TetherNotificationPresenterTest;

  // IDs associated with Tether notification types.
  static const char kPotentialHotspotNotificationId[];
  static const char kActiveHostNotificationId[];
  static const char kSetupRequiredNotificationId[];

  // IDs of all notifications which, when clicked, open mobile data settings.
  static const char* const kIdsWhichOpenTetherSettingsOnClick[];

  // Reflects InstantTethering_NotificationInteractionType enum in enums.xml. Do
  // not rearrange.
  enum NotificationInteractionType {
    NOTIFICATION_BODY_TAPPED_SINGLE_HOST_NEARBY = 0,
    NOTIFICATION_BODY_TAPPED_MULTIPLE_HOSTS_NEARBY = 1,
    NOTIFICATION_BODY_TAPPED_SETUP_REQUIRED = 2,
    NOTIFICATION_BODY_TAPPED_CONNECTION_FAILED = 3,
    NOTIFICATION_BUTTON_TAPPED_HOST_NEARBY = 4,
    NOTIFICATION_INTERACTION_TYPE_MAX
  };

  void OnNotificationClicked(const std::string& notification_id,
                             std::optional<int> button_index);
  NotificationInteractionType GetMetricValueForClickOnNotificationBody(
      const std::string& clicked_notification_id) const;
  void OnNotificationClosed(const std::string& notification_id);

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const NotificationCatalogName& catalog_name,
      const std::u16string& title,
      const std::u16string& message,
      const gfx::ImageSkia& small_image,
      const message_center::RichNotificationData& rich_notification_data);

  void SetSettingsUiDelegateForTesting(
      std::unique_ptr<SettingsUiDelegate> settings_ui_delegate);
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);
  void OpenSettingsAndRemoveNotification(const std::string& settings_subpage,
                                         const std::string& notification_id);
  void RemoveNotificationIfVisible(const std::string& notification_id);

  bool AreNotificationsEnabled();

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<NetworkConnect, DanglingUntriaged> network_connect_;

  // The ID of the currently showing notification.
  std::string showing_notification_id_;

  std::unique_ptr<SettingsUiDelegate> settings_ui_delegate_;

  // The device ID of the device whose metadata is displayed in the "potential
  // hotspot nearby" notification. If the notification is not visible or it is
  // in the "multiple hotspots available" mode, this pointer is null.
  std::unique_ptr<std::string> hotspot_nearby_device_id_;

  base::WeakPtrFactory<TetherNotificationPresenter> weak_ptr_factory_{this};
};

}  // namespace ash::tether

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_TETHER_NOTIFICATION_PRESENTER_H_
