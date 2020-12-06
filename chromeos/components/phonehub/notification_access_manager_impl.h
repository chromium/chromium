// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_

#include "chromeos/components/phonehub/notification_access_manager.h"

#include "chromeos/components/phonehub/feature_status_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace phonehub {

class MessageSender;
class ConnectionScheduler;

// Implements NotificationAccessManager by persisting the last-known
// notification access value to user prefs.
class NotificationAccessManagerImpl : public NotificationAccessManager,
                                      public FeatureStatusProvider::Observer {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit NotificationAccessManagerImpl(
      PrefService* pref_service,
      FeatureStatusProvider* feature_status_provider,
      MessageSender* message_sender,
      ConnectionScheduler* connection_scheduler);
  ~NotificationAccessManagerImpl() override;

 private:
  friend class NotificationAccessManagerImplTest;

  // NotificationAccessManager:
  AccessStatus GetAccessStatus() const override;
  void SetAccessStatusInternal(AccessStatus access_status) override;
  void OnSetupRequested() override;

  bool HasNotificationSetupUiBeenDismissed() const override;
  void DismissSetupRequiredUi() override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void SendShowNotificationAccessSetupRequest();

  FeatureStatus current_feature_status_;
  PrefService* pref_service_;
  FeatureStatusProvider* feature_status_provider_;
  MessageSender* message_sender_;
  ConnectionScheduler* connection_scheduler_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_ACCESS_MANAGER_IMPL_H_
