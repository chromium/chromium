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
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/mobile/mobile_activator.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/ash/network/network_portal_signin_controller.h"
#include "chrome/grit/generated_resources.h"
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

std::unique_ptr<message_center::Notification> CreatePost2022Notification(
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
      message = IDS_NEW_PORTAL_DETECTION_NOTIFICATION_MESSAGE;
      button.title = l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_BUTTON);
      data.buttons.emplace_back(std::move(button));
      break;
    case NetworkState::PortalState::kPortalSuspected:
      message = IDS_NEW_PORTAL_SUSPECTED_DETECTION_NOTIFICATION_MESSAGE;
      button.title = l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_SUSPECTED_DETECTION_NOTIFICATION_BUTTON);
      data.buttons.emplace_back(std::move(button));
      break;
    case NetworkState::PortalState::kProxyAuthRequired:
      message =
          IDS_NEW_PORTAL_PROXY_AUTH_REQUIRED_DETECTION_NOTIFICATION_MESSAGE;
      button.title = l10n_util::GetStringUTF16(
          IDS_NEW_PORTAL_DETECTION_NOTIFICATION_BUTTON);
      data.buttons.emplace_back(std::move(button));
      break;
    default:
      NOTREACHED();
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

std::unique_ptr<message_center::Notification> CreatePre2022Notification(
    const NetworkState* network,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    message_center::NotifierId notifier_id,
    bool is_wifi) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          NetworkPortalNotificationController::kNotificationId,
          l10n_util::GetStringUTF16(
              is_wifi ? IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIFI
                      : IDS_PORTAL_DETECTION_NOTIFICATION_TITLE_WIRED),
          l10n_util::GetStringFUTF16(
              is_wifi ? IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_WIFI
                      : IDS_PORTAL_DETECTION_NOTIFICATION_MESSAGE_WIRED,
              base::UTF8ToUTF16(network->name())),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          notifier_id, message_center::RichNotificationData(),
          std::move(delegate), kNotificationCaptivePortalIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
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
  explicit NotificationDelegateImpl(
      base::WeakPtr<NetworkPortalSigninController> signin_controller)
      : signin_controller_(signin_controller) {}
  NotificationDelegateImpl(const NotificationDelegateImpl&) = delete;
  NotificationDelegateImpl& operator=(const NotificationDelegateImpl&) = delete;

  // message_center::NotificationDelegate
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

 private:
  ~NotificationDelegateImpl() override = default;

  base::WeakPtr<NetworkPortalSigninController> signin_controller_;
};

void NotificationDelegateImpl::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (signin_controller_)
    signin_controller_->ShowSignin();
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
  if (!network ||
      (portal_state != NetworkState::PortalState::kPortal &&
       portal_state != NetworkState::PortalState::kPortalSuspected &&
       portal_state != NetworkState::PortalState::kProxyAuthRequired)) {
    last_network_guid_.clear();
    last_portal_state_ = portal_state;

    // In browser tests we initiate fake network portal detection, but network
    // state usually stays connected. This way, after dialog is shown, it is
    // immediately closed. The testing check below prevents dialog from closing.
    if (signin_controller_ &&
        (!ignore_no_network_for_testing_ ||
         portal_state == NetworkState::PortalState::kOnline)) {
      signin_controller_->CloseSignin();
    }

    CloseNotification();
    return;
  }

  // Don't do anything if we're currently activating the device.
  if (MobileActivator::GetInstance()->RunningActivation())
    return;

  // Don't do anything if notification for |network| already was
  // displayed with the same portal_state.
  if (network->guid() == last_network_guid_ &&
      portal_state == last_portal_state_) {
    return;
  }
  last_network_guid_ = network->guid();
  last_portal_state_ = portal_state;

  base::UmaHistogramEnumeration("Network.NetworkPortalNotificationState",
                                portal_state);

  std::unique_ptr<message_center::Notification> notification =
      CreateDefaultCaptivePortalNotification(network, portal_state);
  DCHECK(notification) << "Notification not created for portal state: "
                       << portal_state;
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void NetworkPortalNotificationController::OnShuttingDown() {
  if (signin_controller_)
    signin_controller_->CloseSignin();
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void NetworkPortalNotificationController::OnSessionStateChanged() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::LOCKED) {
    if (signin_controller_)
      signin_controller_->CloseSignin();
  }
}

std::unique_ptr<message_center::Notification>
NetworkPortalNotificationController::CreateDefaultCaptivePortalNotification(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  signin_controller_ = std::make_unique<NetworkPortalSigninController>();
  auto notification_delegate = base::MakeRefCounted<NotificationDelegateImpl>(
      signin_controller_->GetWeakPtr());
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kNotifierNetworkPortalDetector,
      NotificationCatalogName::kNetworkPortalDetector);
  bool is_wifi = NetworkTypePattern::WiFi().MatchesType(network->type());
  std::unique_ptr<message_center::Notification> notification;
  if (features::IsCaptivePortalUI2022Enabled()) {
    notification = CreatePost2022Notification(
        network, notification_delegate, notifier_id, is_wifi, portal_state);
  } else {
    notification = CreatePre2022Notification(network, notification_delegate,
                                             notifier_id, is_wifi);
  }
  return notification;
}

void NetworkPortalNotificationController::SetIgnoreNoNetworkForTesting() {
  ignore_no_network_for_testing_ = true;
}

bool NetworkPortalNotificationController::IsDialogShownForTesting() const {
  return signin_controller_ && signin_controller_->DialogIsShown();
}

}  // namespace ash
