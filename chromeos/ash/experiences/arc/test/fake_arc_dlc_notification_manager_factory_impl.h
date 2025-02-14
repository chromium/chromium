// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_

#include <memory>

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_notification_manager_factory.h"

class AccountId;

namespace arc {

// This class is a test implementation of ArcDlcNotificationManagerFactory that
// provides a fake notification manager for ARC DLC installation. It allows
// tests to simulate notifications without requiring real system interactions.
class ArcDlcInstallNotificationManager;
class FakeArcDlcNotificationManagerFactoryImpl
    : public ArcDlcNotificationManagerFactory {
 public:
  FakeArcDlcNotificationManagerFactoryImpl();

  FakeArcDlcNotificationManagerFactoryImpl(
      const FakeArcDlcNotificationManagerFactoryImpl&) = delete;
  FakeArcDlcNotificationManagerFactoryImpl& operator=(
      const FakeArcDlcNotificationManagerFactoryImpl&) = delete;

  ~FakeArcDlcNotificationManagerFactoryImpl() override;
  std::unique_ptr<ArcDlcInstallNotificationManager> CreateNotificationManager(
      const AccountId& account_id) override;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_
