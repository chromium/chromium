// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/mobile/mobile_activator.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "components/session_manager/core/session_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const char kNotifierNetworkPortalDetector[] = "ash.network.portal-detector";

bool IsPortalState(NetworkState::PortalState portal_state) {
  return portal_state == NetworkState::PortalState::kPortal ||
         portal_state == NetworkState::PortalState::kPortalSuspected;
}

std::unique_ptr<message_center::Notification> CreateNotification(
    const NetworkState* network,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    message_center::NotifierId notifier_id,
    bool is_wifi,
    NetworkState::PortalState portal_state) {
  int message;
  message_center::ButtonInfo button;
  message_center::RichNotificationData data;
  switch (portal_state) {
    case NetworkState::PortalState::kPortal:
    case NetworkState::PortalState::kPortalSuspected:
      message = IDS_NEW_PORTAL_DETECTION_NOTIFICATION_MESSAGE;
      button.title = l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_BUTTON);
      data.buttons.emplace_back(std::move(button));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          NetworkPortalNotificationController::kNotificationId,
          l10n_util::GetStringUTF16(
              is_wifi ? IDS_NEW_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI
                      : IDS_NEW_PORTAL_DETECTION_NOTIFICATION_TITLE_WIRED),
          l10n_util::GetStringFUTF16(message,
                                     base::UTF8ToUTF16(network->name())),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          notifier_id, data, std::move(delegate),
          kNotificationCaptivePortalIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_never_timeout(true);
  return notification;
}

void CloseNotification() {
  SystemNotificationHelper::GetInstance()->Close(
      NetworkPortalNotificationController::kNotificationId);
}

}  // namespace

class NotificationDelegateImpl : public message_center::NotificationDelegate {
 public:
  NotificationDelegateImpl() = default;
  NotificationDelegateImpl(const NotificationDelegateImpl&) = delete;
  NotificationDelegateImpl& operator=(const NotificationDelegateImpl&) = delete;

  // message_center::NotificationDelegate
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  ~NotificationDelegateImpl() override = default;
};

void NotificationDelegateImpl::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  NET_LOG(USER) << "Captive Portal notification: Click";
  NetworkPortalSigninController::Get()->ShowSignin(
      NetworkPortalSigninController::SigninSource::kNotification);
  CloseNotification();
}

// static
const char NetworkPortalNotificationController::kNotificationId[] =
    "chrome://net/network_portal_detector";

NetworkPortalNotificationController::NetworkPortalNotificationController() {
  if (NetworkHandler::IsInitialized())  // May be null in tests.
    NetworkHandler::Get()->network_state_handler()->AddObserver(this);
  DCHECK(session_manager::SessionManager::Get());
  session_manager::SessionManager::Get()->AddObserver(this);
}

NetworkPortalNotificationController::~NetworkPortalNotificationController() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  }
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);
}

void NetworkPortalNotificationController::PortalStateChanged(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  if (!network || !IsPortalState(portal_state)) {
    if (!last_network_guid_.empty() && IsPortalState(last_portal_state_)) {
      NET_LOG(EVENT) << "Captive Portal notification: Close for "
                     << last_network_guid_;
    }
    last_network_guid_.clear();
    last_portal_state_ = portal_state;

    // In browser tests we initiate fake network portal detection, but network
    // state usually stays connected. This way, after dialog is shown, it is
    // immediately closed. The testing check below prevents dialog from closing.
    if (!ignore_no_network_for_testing_ ||
        portal_state == NetworkState::PortalState::kOnline) {
      NetworkPortalSigninController::Get()->CloseSignin();
    }

    CloseNotification();
    return;
  }

  // Don't do anything if we're currently activating the device.
  if (MobileActivator::GetInstance()->RunningActivation()) {
    NET_LOG(EVENT) << "Captive Portal notification: Skip (mobile activation)";
    return;
  }

  // Don't do anything if notification for |network| already was
  // displayed with the same portal_state.
  if (network->guid() == last_network_guid_ &&
      portal_state == last_portal_state_) {
    return;
  }
  last_network_guid_ = network->guid();
  last_portal_state_ = portal_state;

  NET_LOG(EVENT) << "Captive Portal notification: Show for "
                 << NetworkId(network) << " PortalState: " << portal_state;
  base::UmaHistogramEnumeration("Network.NetworkPortalNotificationState",
                                portal_state);

  std::unique_ptr<message_center::Notification> notification =
      CreateDefaultCaptivePortalNotification(network, portal_state);
  DCHECK(notification) << "Notification not created for portal state: "
                       << portal_state;
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void NetworkPortalNotificationController::OnShuttingDown() {
  NetworkPortalSigninController::Get()->CloseSignin();
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void NetworkPortalNotificationController::OnSessionStateChanged() {
  TRACE_EVENT0("ui",
               "NetworkPortalNotificationController::OnSessionStateChanged");
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::LOCKED) {
    NetworkPortalSigninController::Get()->CloseSignin();
  }
}

std::unique_ptr<message_center::Notification>
NetworkPortalNotificationController::CreateDefaultCaptivePortalNotification(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  auto notification_delegate = base::MakeRefCounted<NotificationDelegateImpl>();
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kNotifierNetworkPortalDetector,
      NotificationCatalogName::kNetworkPortalDetector);
  bool is_wifi = NetworkTypePattern::WiFi().MatchesType(network->type());
  std::unique_ptr<message_center::Notification> notification;
  notification = CreateNotification(network, notification_delegate, notifier_id,
                                    is_wifi, portal_state);
  return notification;
}

void NetworkPortalNotificationController::SetIgnoreNoNetworkForTesting() {
  ignore_no_network_for_testing_ = true;
}

}  // namespace ash
