// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/esim_interactive_uitest_base.h"

#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "dbus/object_path.h"

namespace ash {

void EsimInteractiveUiTestBase::SetUpOnMainThread() {
  InteractiveAshTest::SetUpOnMainThread();

  // Set up context for element tracking for InteractiveBrowserTest.
  SetupContextWidget();

  // Ensure the OS Settings app is installed.
  InstallSystemApps();

  auto* hermes_manager_client = HermesManagerClient::Get()->GetTestInterface();
  DCHECK(hermes_manager_client);

  hermes_manager_client->ClearEuiccs();

  hermes_manager_client->AddEuicc(dbus::ObjectPath(euicc_info_.path()),
                                  euicc_info_.eid(),
                                  /*is_active=*/true,
                                  /*physical_slot=*/0);
}

}  // namespace ash
