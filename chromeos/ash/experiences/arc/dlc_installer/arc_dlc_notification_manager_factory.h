// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_H_

#include <memory>

class AccountId;

namespace arc {

class ArcDlcInstallNotificationManager;

// Interface for handling the creation of notification managers.
// This interface is used to abstract the creation of notification managers
// based on the AccountId for the ARC DLC installer.
class ArcDlcNotificationManagerFactory {
 public:
  virtual ~ArcDlcNotificationManagerFactory() = default;

  // Initializes and returns a notification manager.
  virtual std::unique_ptr<ArcDlcInstallNotificationManager>
  CreateNotificationManager(const AccountId& account_id) = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_H_
