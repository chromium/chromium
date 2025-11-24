// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/desktop_to_mobile_promos/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"

IOSPromoTriggerService::IOSPromoTriggerService(Profile* profile)
    : profile_(profile) {}
IOSPromoTriggerService::~IOSPromoTriggerService() = default;

void IOSPromoTriggerService::NotifyPromoShouldBeShown(
    desktop_to_mobile_promos::PromoType promo_type) {
  callback_list_.Notify(promo_type);
}

const syncer::DeviceInfo* IOSPromoTriggerService::GetIOSDeviceToRemind() {
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
  // TODO(crbug.com/442561857): Trigger iOS push notification as well.
  base::Value::Dict promo_reminder_data;
  promo_reminder_data.Set(prefs::kIOSPromoReminderPromoType,
                          static_cast<int>(promo_type));
  promo_reminder_data.Set(prefs::kIOSPromoReminderDeviceGUID, device_guid);
  profile_->GetPrefs()->SetDict(prefs::kIOSPromoReminder,
                                std::move(promo_reminder_data));
}

base::CallbackListSubscription IOSPromoTriggerService::RegisterPromoCallback(
    PromoCallback callback) {
  return callback_list_.Add(std::move(callback));
}
