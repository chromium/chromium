// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/net/network_portal_web_dialog.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class NetworkPortalNotificationControllerTest;

// Shows a message center notification when the networking stack detects a
// captive portal.
class NetworkPortalNotificationController
    : public NetworkStateHandlerObserver,
      public session_manager::SessionManagerObserver,
      public NetworkPortalWebDialog::Delegate {
 public:
  // The values of these metrics are being used for UMA gathering, so it is
  // important that they don't change between releases.
  enum UserActionMetric {
    USER_ACTION_METRIC_CLICKED,
    USER_ACTION_METRIC_CLOSED,
    USER_ACTION_METRIC_IGNORED,
    USER_ACTION_METRIC_COUNT
  };

  static const char kNotificationId[];

  NetworkPortalNotificationController();

  NetworkPortalNotificationController(
      const NetworkPortalNotificationController&) = delete;
  NetworkPortalNotificationController& operator=(
      const NetworkPortalNotificationController&) = delete;

  ~NetworkPortalNotificationController() override;

  // NetworkPortalWebDialog::Delegate
  void OnDialogDestroyed(const NetworkPortalWebDialog* dialog) override;

  // Ignores "No network" errors in browser tests.
  void SetIgnoreNoNetworkForTesting();

  bool IsDialogShownForTesting() const;

 private:
  class Delegate;
  friend NetworkPortalNotificationControllerTest;

  // Creates NetworkPortalWebDialog.
  void ShowDialog();

  // Destroys NetworkPortalWebDialog.
  void CloseDialog();

  // Creates the default notification informing the user that a captive portal
  // has been detected. On click the captive portal login page is opened in the
  // browser.
  std::unique_ptr<message_center::Notification>
  CreateDefaultCaptivePortalNotification(
      const NetworkState* network,
      NetworkState::PortalState portal_state);

  // NetworkStateHandlerObserver:
  void PortalStateChanged(const NetworkState* network,
                          NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Last network guid for which notification was displayed.
  std::string last_network_guid_;

  // Currently displayed authorization dialog, or NULL if none.
  NetworkPortalWebDialog* dialog_ = nullptr;

  // Do not close Portal Login dialog on "No network" error in browser tests.
  bool ignore_no_network_for_testing_ = false;

  base::WeakPtrFactory<NetworkPortalNotificationController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
