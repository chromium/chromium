// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {
class User;
}

// This class is responsible for triggering a one-time (per user) mobile data
// usage warning notification.
// Shows notification to authenticated users with unlocked screen
// while cellular is the default network and there are no other network
// connection requests.
// Connection status could change before the default network is updated. We
// could run into situations where notifications are shown even though default
// network is about to change to  a non-cellular network. We introduce a delay
// in operations that we think might run into this issue.
class MobileDataNotifications
    : public ash::NetworkStateHandlerObserver,
      public ash::NetworkConnectionObserver,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver {
 public:
  MobileDataNotifications();

  MobileDataNotifications(const MobileDataNotifications&) = delete;
  MobileDataNotifications& operator=(const MobileDataNotifications&) = delete;

  ~MobileDataNotifications() override;

  // NetworkStateHandlerObserver:
  void ActiveNetworksChanged(
      const std::vector<const ash::NetworkState*>& active_networks) override;
  void OnShuttingDown() override;

  // NetworkConnectionObserver:
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  // UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  // Requests the active networks and calls
  // ShowOptionalMobileDataNotificationImpl.
  void ShowOptionalMobileDataNotification();

  // Displays a mobile data warning notification if all conditions are met:
  // * Cellular is the default network.
  // * User is authenticated with unlocked screen.
  // * There are no pending connection requests (Prevent flaky network switches
  //   from triggering the notification).
  // * First time notification is shown according to user prefs.
  void ShowOptionalMobileDataNotificationImpl(
      const std::vector<const ash::NetworkState*>& active_networks);

  // Adds a delay before calling |ShowOptionalMobileDataNotification|. Delay is
  // introduced because in some cases we might be notified through an observer
  // of an update that is not fully propagated to other parts of the
  // system. Checks performed in |ShowOptionalMobileDataNotification| might
  // accidentally pass.
  void DelayedShowOptionalMobileDataNotification();

  base::OneShotTimer one_shot_notification_check_delay_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<MobileDataNotifications> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_
