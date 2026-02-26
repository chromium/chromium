// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/sharing_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "ui/base/l10n/l10n_util.h"

// The value corresponds to the iOS push notification client ID.
// TODO(crbug.com/438769954): Consider moving the PushNotificationClientId enum
// to components so it can be shared between iOS and Desktop.
constexpr char kCrossPlatformPromosClientId[] = "8";

IOSPromoTriggerService::IOSPromoTriggerService(Profile* profile)
    : profile_(profile) {
  auto* collection = ProfileBrowserCollection::GetForProfile(profile_);
  browser_collection_observer_.Observe(collection);

  // Register as an observer for all existing browsers in this profile.
  collection->ForEach([this](BrowserWindowInterface* browser) {
    OnBrowserCreated(browser);
    return true;
  });
}
IOSPromoTriggerService::~IOSPromoTriggerService() = default;

void IOSPromoTriggerService::NotifyPromoShouldBeShown(
    desktop_to_mobile_promos::PromoType promo_type) {
  callback_list_.Notify(promo_type);
}

const syncer::DeviceInfo* IOSPromoTriggerService::GetIOSDeviceToRemind() {
  if (GetMobilePromoOnDesktopForcePromoType() ==
      IOSPromoBubbleForceType::kQRCode) {
    return nullptr;
  }

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile_);
  if (!device_info_sync_service) {
    return nullptr;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  if (!device_info_tracker) {
    return nullptr;
  }

  const syncer::DeviceInfo* preferred_device = nullptr;

  for (const syncer::DeviceInfo* device :
       device_info_tracker->GetAllDeviceInfo()) {
    // Skip non-iOS devices.
    if (device->os_type() != syncer::DeviceInfo::OsType::kIOS) {
      continue;
    }

    if (IsMorePreferredDevice(device, preferred_device)) {
      preferred_device = device;
    }
  }

  return preferred_device;
}

void IOSPromoTriggerService::OnTabGroupChanged(const TabGroupChange& change) {
  if (MobilePromoOnDesktopTypeEnabled(
          MobilePromoOnDesktopPromoType::kTabGroups)) {
    NotifyPromoShouldBeShown(desktop_to_mobile_promos::PromoType::kTabGroups);
  }
}

void IOSPromoTriggerService::OnBrowserCreated(BrowserWindowInterface* browser) {
  browser->GetTabStripModel()->AddObserver(this);
}

void IOSPromoTriggerService::OnBrowserClosed(BrowserWindowInterface* browser) {
  browser->GetTabStripModel()->RemoveObserver(this);
}

bool IOSPromoTriggerService::IsMorePreferredDevice(
    const syncer::DeviceInfo* current_preference,
    const syncer::DeviceInfo* another_device) {
  if (!another_device) {
    return true;
  }
  if (!current_preference) {
    return false;
  }

  // iPhone is always preferred over iPad.
  if (current_preference->form_factor() ==
          syncer::DeviceInfo::FormFactor::kPhone &&
      another_device->form_factor() ==
          syncer::DeviceInfo::FormFactor::kTablet) {
    return true;
  }
  if (current_preference->form_factor() ==
          syncer::DeviceInfo::FormFactor::kTablet &&
      another_device->form_factor() == syncer::DeviceInfo::FormFactor::kPhone) {
    return false;
  }

  // If same form factor, prefer the more recently updated device.
  return current_preference->last_updated_timestamp() >
         another_device->last_updated_timestamp();
}

void IOSPromoTriggerService::SetReminderForIOSDevice(
    desktop_to_mobile_promos::PromoType promo_type,
    const std::string& device_guid) {
  // Set the prefs for the in-app notification.
  base::DictValue promo_reminder_data;
  promo_reminder_data.Set(prefs::kIOSPromoReminderPromoType,
                          static_cast<int>(promo_type));
  promo_reminder_data.Set(prefs::kIOSPromoReminderDeviceGUID, device_guid);
  profile_->GetPrefs()->SetDict(prefs::kIOSPromoReminder,
                                std::move(promo_reminder_data));

  // Create and send payload for push notification.
  if (IsMobilePromoOnDesktopNotificationsEnabled()) {
    SharingService* sharing_service =
        SharingServiceFactory::GetForBrowserContext(profile_);
    std::optional<SharingTargetDeviceInfo> device_info =
        sharing_service->GetDeviceByGuid(device_guid);
    if (device_info) {
      sharing_service->SendUnencryptedMessageToDevice(
          *device_info, CreateNotificationPayload(promo_type, device_guid),
          base::DoNothing());
    }
  }
}

base::CallbackListSubscription IOSPromoTriggerService::RegisterPromoCallback(
    PromoCallback callback) {
  return callback_list_.Add(std::move(callback));
}

sync_pb::UnencryptedSharingMessage
IOSPromoTriggerService::CreateNotificationPayload(
    desktop_to_mobile_promos::PromoType promo_type,
    const std::string& device_guid) {
  int title_id;
  int body_id;
  switch (promo_type) {
    case desktop_to_mobile_promos::PromoType::kPassword:
      title_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_NOTIFICATION_TITLE;
      body_id = IDS_IOS_DESKTOP_PASSWORD_PROMO_NOTIFICATION_BODY;
      break;
    case desktop_to_mobile_promos::PromoType::kEnhancedBrowsing:
      title_id = IDS_IOS_DESKTOP_SAFE_BROWSING_PROMO_NOTIFICATION_TITLE;
      body_id = IDS_IOS_DESKTOP_SAFE_BROWSING_PROMO_NOTIFICATION_BODY;
      break;
    case desktop_to_mobile_promos::PromoType::kLens:
      title_id = IDS_IOS_DESKTOP_LENS_PROMO_NOTIFICATION_TITLE;
      body_id = IDS_IOS_DESKTOP_LENS_PROMO_NOTIFICATION_BODY;
      break;
    default:
      NOTREACHED();
  }

  sync_pb::UnencryptedSharingMessage sharing_message;
  sync_pb::DesktopToMobilePromoMessage* promo_message =
      sharing_message.mutable_desktop_to_mobile_promo_message();
  sync_pb::PushNotificationMessage* push_notification =
      promo_message->mutable_push_notification();

  promo_message->set_promo_type(static_cast<int>(promo_type));
  push_notification->set_title(l10n_util::GetStringUTF8(title_id));
  push_notification->set_text(l10n_util::GetStringUTF8(body_id));
  push_notification->set_push_notification_client_id(
      kCrossPlatformPromosClientId);

  return sharing_message;
}
