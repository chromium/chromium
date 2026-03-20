// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_UPDATE_NOTIFICATION_H_
#define CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_UPDATE_NOTIFICATION_H_

#include <string>

#include "components/prefs/pref_service.h"
#include "ui/message_center/public/cpp/notification.h"

class PrefRegistrySimple;

namespace ash {

// FrozenUpdateNotification is created when user logs in. It is used to check
// current if the device has updates frozen, and  show warning notifications
// accordingly.
class FrozenUpdateNotification final
    : public message_center::NotificationObserver {
 public:
  // Returns true if the notification needs to be displayed.
  static bool ShouldShowFrozenUpdateNotification(PrefService& prefs);

  // PrefService for the user's profile pref
  // `prefs` must not be nullptr, and must outlive this instance.
  explicit FrozenUpdateNotification(PrefService& prefs);

  FrozenUpdateNotification(const FrozenUpdateNotification&) = delete;
  FrozenUpdateNotification& operator=(const FrozenUpdateNotification&) = delete;

  ~FrozenUpdateNotification();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Checks if the device has an affected GPU and if so displays the
  // notification.
  void MaybeShowNotification();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

  void OverrideGpuForTesting(unsigned int vendor, unsigned int device);

 private:
  friend class FrozenUpdateNotificationTestBase;

  static constexpr char kFrozenUpdateNotificationId[] =
      "chrome://product_frozen_update";

  // Buttons that appear in the notification.  This is exposed for testing
  // purposes only and should never be directly used.
  static constexpr size_t kMoreInfoButtonIndex = 0;
  static constexpr size_t kDismissButtonIndex = 1;

  // Display the notification to the user.
  void ShowNotification();

  // PrefService which is associated with the notification.
  const raw_ref<PrefService> prefs_;

  // Override values for testing
  std::optional<unsigned int> test_vendor_;
  std::optional<unsigned int> test_device_;

  // Factory of callbacks.
  base::WeakPtrFactory<FrozenUpdateNotification> weak_ptr_factory_{this};
};

}  // namespace ash
#endif  // CHROMEOS_ASH_EXPERIENCES_FROZEN_UPDATE_FROZEN_UPDATE_NOTIFICATION_H_
