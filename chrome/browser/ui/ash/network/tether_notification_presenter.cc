// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"

#include <algorithm>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash::tether {

namespace {

const char kNotifierTether[] = "ash.tether";

// Mean value of NetworkState's signal_strength() range.
const int kMediumSignalStrength = 50;

// Dimensions of Tether notification icon in pixels.
constexpr gfx::Size kTetherSignalIconSize(18, 18);

// Handles clicking and closing of a notification via callbacks.
class TetherNotificationDelegate
    : public message_center::HandleNotificationClickDelegate {
 public:
  TetherNotificationDelegate(ButtonClickCallback click,
                             base::RepeatingClosure close)
      : HandleNotificationClickDelegate(click), close_callback_(close) {}

  TetherNotificationDelegate(const TetherNotificationDelegate&) = delete;
  TetherNotificationDelegate& operator=(const TetherNotificationDelegate&) =
      delete;

  // NotificationDelegate:
  void Close(bool by_user) override {
    if (!close_callback_.is_null()) {
      close_callback_.Run();
    }
  }

 private:
  ~TetherNotificationDelegate() override = default;

  base::RepeatingClosure close_callback_;
};

class SettingsUiDelegateImpl
    : public TetherNotificationPresenter::SettingsUiDelegate {
 public:
  SettingsUiDelegateImpl() = default;
  ~SettingsUiDelegateImpl() override = default;

  void ShowSettingsSubPageForProfile(Profile* profile,
                                     const std::string& sub_page) override {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                                 sub_page);
  }
};

// Returns the icon to use for a network with the given signal strength, which
// should range from 0 to 100 (inclusive).
const gfx::ImageSkia GetImageForSignalStrength(int signal_strength) {
  // Convert the [0, 100] range to [0, 4], since there are 5 distinct signal
  // strength icons (0 bars to 4 bars).
  int normalized_signal_strength = std::clamp(signal_strength / 25, 0, 4);

  return gfx::CanvasImageSource::MakeImageSkia<
      network_icon::SignalStrengthImageSource>(
      network_icon::BARS, gfx::kGoogleBlue500, kTetherSignalIconSize,
      normalized_signal_strength, 5);
}

}  // namespace

// static
constexpr const char TetherNotificationPresenter::kActiveHostNotificationId[] =
    "cros_tether_notification_ids.active_host";

// static
constexpr const char
    TetherNotificationPresenter::kPotentialHotspotNotificationId[] =
        "cros_tether_notification_ids.potential_hotspot";

// static
constexpr const char
    TetherNotificationPresenter::kSetupRequiredNotificationId[] =
        "cros_tether_notification_ids.setup_required";

// static
constexpr const char* const
    TetherNotificationPresenter::kIdsWhichOpenTetherSettingsOnClick[] = {
        TetherNotificationPresenter::kActiveHostNotificationId,
        TetherNotificationPresenter::kPotentialHotspotNotificationId,
        TetherNotificationPresenter::kSetupRequiredNotificationId};

TetherNotificationPresenter::TetherNotificationPresenter(
    Profile* profile,
    NetworkConnect* network_connect)
    : profile_(profile),
      network_connect_(network_connect),
      settings_ui_delegate_(base::WrapUnique(new SettingsUiDelegateImpl())) {}

TetherNotificationPresenter::~TetherNotificationPresenter() = default;

// static
void TetherNotificationPresenter::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterBooleanPref(prefs::kNotificationsEnabled, true);
}

void TetherNotificationPresenter::NotifyPotentialHotspotNearby(
    const std::string& device_id,
    const std::string& device_name,
    int signal_strength) {
  PA_LOG(VERBOSE) << "Displaying \"potential hotspot nearby\" notification for "
                  << "device with name \"" << device_name << "\". "
                  << "Notification ID = " << kPotentialHotspotNotificationId;

  hotspot_nearby_device_id_ = std::make_unique<std::string>(device_id);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_CONNECT)));

  ShowNotification(CreateNotification(
      kPotentialHotspotNotificationId,
      NotificationCatalogName::kTetherPotentialHotspot,
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_ONE_DEVICE_MESSAGE,
          base::ASCIIToUTF16(device_name)),
      GetImageForSignalStrength(signal_strength), rich_notification_data));
}

void TetherNotificationPresenter::NotifyMultiplePotentialHotspotsNearby() {
  PA_LOG(VERBOSE) << "Displaying \"potential hotspot nearby\" notification for "
                  << "multiple devices. Notification ID = "
                  << kPotentialHotspotNotificationId;

  hotspot_nearby_device_id_.reset();

  ShowNotification(CreateNotification(
      kPotentialHotspotNotificationId,
      NotificationCatalogName::kTetherPotentialHotspot,
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_TITLE),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_WIFI_AVAILABLE_MULTIPLE_DEVICES_MESSAGE),
      GetImageForSignalStrength(kMediumSignalStrength),
      {} /* rich_notification_data */));
}

NotificationPresenter::PotentialHotspotNotificationState
TetherNotificationPresenter::GetPotentialHotspotNotificationState() {
  if (showing_notification_id_ != kPotentialHotspotNotificationId) {
    return NotificationPresenter::PotentialHotspotNotificationState::
        NO_HOTSPOT_NOTIFICATION_SHOWN;
  }

  return hotspot_nearby_device_id_
             ? NotificationPresenter::PotentialHotspotNotificationState::
                   SINGLE_HOTSPOT_NEARBY_SHOWN
             : NotificationPresenter::PotentialHotspotNotificationState::
                   MULTIPLE_HOTSPOTS_NEARBY_SHOWN;
}

void TetherNotificationPresenter::RemovePotentialHotspotNotification() {
  RemoveNotificationIfVisible(kPotentialHotspotNotificationId);
}

void TetherNotificationPresenter::NotifySetupRequired(
    const std::string& device_name,
    int signal_strength) {
  PA_LOG(VERBOSE) << "Displaying \"setup required\" notification. Notification "
                  << "ID = " << kSetupRequiredNotificationId;

  // Persist this notification until acted upon or dismissed, so that the user
  // is aware that they need to complete setup on their phone.
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.never_timeout = true;

  ShowNotification(CreateNotification(
      kSetupRequiredNotificationId,
      NotificationCatalogName::kTetherSetupRequired,
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_TITLE,
                                 base::ASCIIToUTF16(device_name)),
      l10n_util::GetStringFUTF16(IDS_TETHER_NOTIFICATION_SETUP_REQUIRED_MESSAGE,
                                 base::ASCIIToUTF16(device_name)),
      GetImageForSignalStrength(signal_strength), rich_notification_data));
}

void TetherNotificationPresenter::RemoveSetupRequiredNotification() {
  RemoveNotificationIfVisible(kSetupRequiredNotificationId);
}

void TetherNotificationPresenter::NotifyConnectionToHostFailed() {
  const std::string id = kActiveHostNotificationId;
  PA_LOG(VERBOSE) << "Displaying \"connection attempt failed\" notification. "
                  << "Notification ID = " << id;

  ShowNotification(CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id,
      features::IsInstantHotspotRebrandEnabled()
          ? l10n_util::GetStringUTF16(
                IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_TITLE)
          : l10n_util::GetStringUTF16(
                IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_TITLE_LEGACY),
      l10n_util::GetStringUTF16(
          IDS_TETHER_NOTIFICATION_CONNECTION_FAILED_MESSAGE),
      std::u16string() /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierTether,
          NotificationCatalogName::kTetherConnectionError),
      {} /* rich_notification_data */,
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &TetherNotificationPresenter::OnNotificationClicked,
          weak_ptr_factory_.GetWeakPtr(), id)),
      kNotificationCellularAlertIcon,
      message_center::SystemNotificationWarningLevel::WARNING));
}

void TetherNotificationPresenter::RemoveConnectionToHostFailedNotification() {
  RemoveNotificationIfVisible(kActiveHostNotificationId);
}

void TetherNotificationPresenter::OnNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  if (button_index) {
    DCHECK_EQ(kPotentialHotspotNotificationId, notification_id);
    DCHECK_EQ(0, *button_index);
    DCHECK(hotspot_nearby_device_id_);
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.NotificationInteractionType",
        TetherNotificationPresenter::NOTIFICATION_BUTTON_TAPPED_HOST_NEARBY,
        TetherNotificationPresenter::NOTIFICATION_INTERACTION_TYPE_MAX);
    PA_LOG(VERBOSE) << "\"Potential hotspot nearby\" notification button was "
                    << "clicked.";
    network_connect_->ConnectToNetworkId(*hotspot_nearby_device_id_);
    RemoveNotificationIfVisible(kPotentialHotspotNotificationId);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.NotificationInteractionType",
      GetMetricValueForClickOnNotificationBody(notification_id),
      TetherNotificationPresenter::NOTIFICATION_INTERACTION_TYPE_MAX);

  OpenSettingsAndRemoveNotification(
      chromeos::settings::mojom::kMobileDataNetworksSubpagePath,
      notification_id);
}

TetherNotificationPresenter::NotificationInteractionType
TetherNotificationPresenter::GetMetricValueForClickOnNotificationBody(
    const std::string& clicked_notification_id) const {
  if (clicked_notification_id == kPotentialHotspotNotificationId &&
      hotspot_nearby_device_id_.get()) {
    return TetherNotificationPresenter::
        NOTIFICATION_BODY_TAPPED_SINGLE_HOST_NEARBY;
  }
  if (clicked_notification_id == kPotentialHotspotNotificationId &&
      !hotspot_nearby_device_id_.get()) {
    return TetherNotificationPresenter::
        NOTIFICATION_BODY_TAPPED_MULTIPLE_HOSTS_NEARBY;
  }
  if (clicked_notification_id == kSetupRequiredNotificationId) {
    return TetherNotificationPresenter::NOTIFICATION_BODY_TAPPED_SETUP_REQUIRED;
  }
  if (clicked_notification_id == kActiveHostNotificationId) {
    return TetherNotificationPresenter::
        NOTIFICATION_BODY_TAPPED_CONNECTION_FAILED;
  }
  NOTREACHED_IN_MIGRATION();
  return TetherNotificationPresenter::NOTIFICATION_INTERACTION_TYPE_MAX;
}

void TetherNotificationPresenter::OnNotificationClosed(
    const std::string& notification_id) {
  if (showing_notification_id_ == notification_id) {
    showing_notification_id_.clear();
  }
}

std::unique_ptr<message_center::Notification>
TetherNotificationPresenter::CreateNotification(
    const std::string& id,
    const NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const gfx::ImageSkia& small_image,
    const message_center::RichNotificationData& rich_notification_data) {
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
      message, ui::ImageModel(), std::u16string() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierTether, catalog_name),
      rich_notification_data,
      new TetherNotificationDelegate(
          base::BindRepeating(
              &TetherNotificationPresenter::OnNotificationClicked,
              weak_ptr_factory_.GetWeakPtr(), id),
          base::BindRepeating(
              &TetherNotificationPresenter::OnNotificationClosed,
              weak_ptr_factory_.GetWeakPtr(), id)));
  notification->SetSmallImage(gfx::Image(small_image));
  if (base::FeatureList::IsEnabled(ash::features::kInstantHotspotRebrand)) {
    notification->set_never_timeout(true);
  }
  return notification;
}

void TetherNotificationPresenter::SetSettingsUiDelegateForTesting(
    std::unique_ptr<SettingsUiDelegate> settings_ui_delegate) {
  settings_ui_delegate_ = std::move(settings_ui_delegate);
}

void TetherNotificationPresenter::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  if (!AreNotificationsEnabled()) {
    PA_LOG(INFO) << "Not showing notification with ID [" << notification->id()
                 << "] since user has notifications disabled.";
    return;
  }

  showing_notification_id_ = notification->id();
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

void TetherNotificationPresenter::OpenSettingsAndRemoveNotification(
    const std::string& settings_subpage,
    const std::string& notification_id) {
  PA_LOG(VERBOSE) << "Notification with ID " << notification_id
                  << " was clicked. "
                  << "Opening settings subpage: " << settings_subpage;

  settings_ui_delegate_->ShowSettingsSubPageForProfile(profile_,
                                                       settings_subpage);
  RemoveNotificationIfVisible(notification_id);
}

void TetherNotificationPresenter::RemoveNotificationIfVisible(
    const std::string& notification_id) {
  if (notification_id == kPotentialHotspotNotificationId) {
    hotspot_nearby_device_id_.reset();
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);
}

bool TetherNotificationPresenter::AreNotificationsEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kNotificationsEnabled);
}

}  // namespace ash::tether
