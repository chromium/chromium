// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_

#include "chromeos/components/phonehub/notification_access_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace phonehub {

// Implements NotificationAccessManager by persisting the last-known
// notification access value to user prefs.
// TODO(khorimoto): Currently HasAccessBeenGranted() always returns false. Have
// it return true once the phone has sent a message indicating that it has
// granted access.
class NotificationAccessManagerImpl : public NotificationAccessManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit NotificationAccessManagerImpl(PrefService* pref_service);
  ~NotificationAccessManagerImpl() override;

 private:
  // NotificationAccessManager:
  bool HasAccessBeenGranted() const override;
  void SetHasAccessBeenGrantedInternal(bool has_access_been_granted) override;
  void OnSetupAttemptStarted() override;
  void OnSetupAttemptEnded() override;

  PrefService* pref_service_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_
