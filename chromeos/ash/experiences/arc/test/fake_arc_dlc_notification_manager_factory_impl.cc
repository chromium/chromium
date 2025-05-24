// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_notification_manager_factory_impl.h"

#include <utility>

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_dlc_install_notification_delegate.h"

namespace arc {

FakeArcDlcNotificationManagerFactoryImpl::
    FakeArcDlcNotificationManagerFactoryImpl() = default;

FakeArcDlcNotificationManagerFactoryImpl::
    ~FakeArcDlcNotificationManagerFactoryImpl() = default;

std::unique_ptr<ArcDlcInstallNotificationManager>
FakeArcDlcNotificationManagerFactoryImpl::CreateNotificationManager(
    const AccountId& account_id) {
  auto delegate = std::make_unique<FakeArcDlcInstallNotificationDelegate>();
  return std::make_unique<ArcDlcInstallNotificationManager>(std::move(delegate),
                                                            account_id);
}

}  // namespace arc
