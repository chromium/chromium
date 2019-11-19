// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace extensions {
class Extension;
class NetworkingConfigService;
}  // namespace extensions

namespace chromeos {

class NetworkState;
class NetworkPortalWebDialog;
class NetworkPortalNotificationControllerTest;

// Shows a message center notification when the networking stack detects a
// captive portal.
class NetworkPortalNotificationController
    : public NetworkStateHandlerObserver,
      public NetworkPortalDetector::Observer,
      public session_manager::SessionManagerObserver {
 public:
  // The values of these metrics are being used for UMA gathering, so it is
  // important that they don't change between releases.
  enum NotificationMetric {
    NOTIFICATION_METRIC_DISPLAYED = 0,

    // This value is no longer used by is still kept here just for
    // unify with histograms.xml.
    NOTIFICATION_METRIC_ERROR = 1,

    NOTIFICATION_METRIC_COUNT = 2
  };

  enum UserActionMetric {
    USER_ACTION_METRIC_CLICKED,
    USER_ACTION_METRIC_CLOSED,
    USER_ACTION_METRIC_IGNORED,
    USER_ACTION_METRIC_COUNT
  };

  static const int kUseExtensionButtonIndex;
  static const int kOpenPortalButtonIndex;

  static const char kNotificationId[];

  static const char kNotificationMetric[];
  static const char kUserActionMetric[];

  explicit NetworkPortalNotificationController(
      NetworkPortalDetector* network_portal_dectector);
  ~NetworkPortalNotificationController() override;

  // Creates NetworkPortalWebDialog.
  void ShowDialog();

  // Destroys NetworkPortalWebDialog.
  void CloseDialog();

  // NULLifies reference to the active dialog.
  void OnDialogDestroyed(const NetworkPortalWebDialog* dialog);

  // Called if an extension has successfully finished authentication to the
  // previously detected captive portal.
  void OnExtensionFinishedAuthentication();

  // Ignores "No network" errors in browser tests.
  void SetIgnoreNoNetworkForTesting();

  // Browser tests should be able to verify that NetworkPortalWebDialog is
  // shown.
  const NetworkPortalWebDialog* GetDialogForTesting() const;

 private:
  friend NetworkPortalNotificationControllerTest;

  // Creates the default notification informing the user that a captive portal
  // has been detected. On click the captive portal login page is opened in the
  // browser.
  std::unique_ptr<message_center::Notification>
  CreateDefaultCaptivePortalNotification(const NetworkState* network);

  // Creates an advanced captive portal notification informing the user that a
  // captive portal has been detected and an extension has registered to perform
  // captive portal authentication for that network. Gives the user the choice
  // to either authenticate using that extension or open the captive portal
  // login page in the browser.
  std::unique_ptr<message_center::Notification>
  CreateCaptivePortalNotificationForExtension(
      const NetworkState* network,
      extensions::NetworkingConfigService* networking_config_service,
      const extensions::Extension* extension);

  // Constructs a notification to inform the user that a captive portal has been
  // detected.
  std::unique_ptr<message_center::Notification> GetNotification(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state);

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

  // NetworkPortalDetector::Observer:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;
  void OnShutdown() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Last network guid for which notification was displayed.
  std::string last_network_guid_;

  // Backpointer to owner.
  NetworkPortalDetector* network_portal_detector_ = nullptr;

  // Currently displayed authorization dialog, or NULL if none.
  NetworkPortalWebDialog* dialog_ = nullptr;

  // Do not close Portal Login dialog on "No network" error in browser tests.
  bool ignore_no_network_for_testing_ = false;

  base::WeakPtrFactory<NetworkPortalNotificationController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
