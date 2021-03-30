// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/mobile/mobile_activator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/net/network_portal_web_dialog.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const char kNotifierNetworkPortalDetector[] = "ash.network.portal-detector";

void CloseNotification() {
  SystemNotificationHelper::GetInstance()->Close(
      NetworkPortalNotificationController::kNotificationId);
}

class NetworkPortalNotificationControllerDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit NetworkPortalNotificationControllerDelegate(
      const std::string& guid,
      base::WeakPtr<NetworkPortalNotificationController> controller)
      : guid_(guid), clicked_(false), controller_(controller) {}

  // Overridden from message_center::NotificationDelegate:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<std::u16string>& reply) override;

 private:
  ~NetworkPortalNotificationControllerDelegate() override {}

  // GUID of the network this notification is generated for.
  std::string guid_;

  bool clicked_;

  base::WeakPtr<NetworkPortalNotificationController> controller_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationControllerDelegate);
};

void NetworkPortalNotificationControllerDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<std::u16string>& reply) {
  clicked_ = true;

  Profile* profile = ProfileManager::GetActiveUserProfile();

  const bool use_incognito_profile =
      profile && profile->GetPrefs()->GetBoolean(
                     prefs::kCaptivePortalAuthenticationIgnoresProxy);

  if (use_incognito_profile) {
    if (controller_)
      controller_->ShowDialog();
  } else {
    if (!profile)
      return;
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    if (!displayer.browser())
      return;
    GURL url(captive_portal::CaptivePortalDetector::kDefaultURL);
    ShowSingletonTab(displayer.browser(), url);
  }
  CloseNotification();
}

}  // namespace

// static
const char NetworkPortalNotificationController::kNotificationId[] =
    "chrome://net/network_portal_detector";

NetworkPortalNotificationController::NetworkPortalNotificationController(
    NetworkPortalDetector* network_portal_detector)
    : network_portal_detector_(network_portal_detector) {
  if (network_portal_detector_) {  // May be null in tests.
    network_portal_detector_->AddObserver(this);
    DCHECK(session_manager::SessionManager::Get());
    session_manager::SessionManager::Get()->AddObserver(this);
  }
}

NetworkPortalNotificationController::~NetworkPortalNotificationController() {
  if (network_portal_detector_) {
    if (session_manager::SessionManager::Get())
      session_manager::SessionManager::Get()->RemoveObserver(this);
    network_portal_detector_->RemoveObserver(this);
  }
}

void NetworkPortalNotificationController::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  if (!network ||
      status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    last_network_guid_.clear();

    // In browser tests we initiate fake network portal detection, but network
    // state usually stays connected. This way, after dialog is shown, it is
    // immediately closed. The testing check below prevents dialog from closing.
    if (dialog_ &&
        (!ignore_no_network_for_testing_ ||
         status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)) {
      dialog_->Close();
    }

    CloseNotification();
    return;
  }

  // Don't do anything if we're currently activating the device.
  if (ash::MobileActivator::GetInstance()->RunningActivation())
    return;

  // Don't do anything if notification for |network| already was
  // displayed.
  if (network->guid() == last_network_guid_)
    return;
  last_network_guid_ = network->guid();

  std::unique_ptr<message_center::Notification> notification =
      CreateDefaultCaptivePortalNotification(network);
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void NetworkPortalNotificationController::OnShutdown() {
  CloseDialog();
  network_portal_detector_->RemoveObserver(this);
  network_portal_detector_ = nullptr;
}

void NetworkPortalNotificationController::OnSessionStateChanged() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  if (state == session_manager::SessionState::LOCKED)
    CloseDialog();
}

void NetworkPortalNotificationController::ShowDialog() {
  if (dialog_)
    return;

  Profile* signin_profile = ProfileHelper::GetSigninProfile();
  dialog_ = new NetworkPortalWebDialog(weak_factory_.GetWeakPtr());
  dialog_->SetWidget(views::Widget::GetWidgetForNativeWindow(
      chrome::ShowWebDialog(nullptr, signin_profile, dialog_)));
}

void NetworkPortalNotificationController::OnDialogDestroyed(
    const NetworkPortalWebDialog* dialog) {
  if (dialog == dialog_) {
    dialog_ = nullptr;
    ProfileHelper::Get()->ClearSigninProfile(base::NullCallback());
  }
}

std::unique_ptr<message_center::Notification>
NetworkPortalNotificationController::CreateDefaultCaptivePortalNotification(
    const NetworkState* network) {
  auto delegate =
      base::MakeRefCounted<NetworkPortalNotificationControllerDelegate>(
          network->guid(), weak_factory_.GetWeakPtr());
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kNotifierNetworkPortalDetector);
  bool is_wifi = NetworkTypePattern::WiFi().MatchesType(network->type());
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
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
  return notification;
}

void NetworkPortalNotificationController::SetIgnoreNoNetworkForTesting() {
  ignore_no_network_for_testing_ = true;
}

void NetworkPortalNotificationController::CloseDialog() {
  if (dialog_)
    dialog_->Close();
}

const NetworkPortalWebDialog*
NetworkPortalNotificationController::GetDialogForTesting() const {
  return dialog_;
}

}  // namespace chromeos
