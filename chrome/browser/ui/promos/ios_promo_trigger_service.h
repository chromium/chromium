// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_

#include "base/callback_list.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace syncer {
class DeviceInfo;
}  // namespace syncer

// Service that acts as a communication bridge between different UI components
// to manage and trigger iOS promos.
class IOSPromoTriggerService : public KeyedService {
 public:
  using PromoCallback =
      base::RepeatingCallback<void(desktop_to_mobile_promos::PromoType)>;

  explicit IOSPromoTriggerService(Profile* profile);
  ~IOSPromoTriggerService() override;

  IOSPromoTriggerService(const IOSPromoTriggerService&) = delete;
  IOSPromoTriggerService& operator=(const IOSPromoTriggerService&) = delete;

  // Notifies observers that a promo should be shown.
  // TODO(crbug.com/446944658): The NotifyPromoShouldBeShown API is a temporary
  // solution for triggering promos. The long-term plan is to migrate the
  // presentation logic to the Browser User Education system. Once that is
  // complete, it can be removed.
  virtual void NotifyPromoShouldBeShown(
      desktop_to_mobile_promos::PromoType promo_type);

  // Returns a synced iOS device to show a reminder on. This method prioritizes
  // the most recently active iPhone. If no iPhone is found, it falls back to
  // the most recently active iPad. If no iOS devices are found, it returns
  // nullptr.
  virtual const syncer::DeviceInfo* GetIOSDeviceToRemind();

  // Sets a preference to show a reminder for `promo_type` on a synced iOS
  // device with guid `device_guid`.
  virtual void SetReminderForIOSDevice(
      desktop_to_mobile_promos::PromoType promo_type,
      const std::string& device_guid);

  // Registers a callback to be notified when a promo should be shown.
  [[nodiscard]] base::CallbackListSubscription RegisterPromoCallback(
      PromoCallback callback);

 private:
  // Returns true if `current_preference` is a more preferred device than
  // `another_device`. Prioritizes iPhones over iPads, and more recently
  // updated devices if form factors are the same.
  bool IsMorePreferredDevice(const syncer::DeviceInfo* current_preference,
                             const syncer::DeviceInfo* another_device);

  base::RepeatingCallbackList<void(desktop_to_mobile_promos::PromoType)>
      callback_list_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_H_
