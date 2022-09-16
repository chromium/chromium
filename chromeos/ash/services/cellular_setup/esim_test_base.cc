// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_test_base.h"

#include <memory>

#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_manager.h"
#include "chromeos/ash/services/cellular_setup/esim_test_utils.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"

namespace ash::cellular_setup {

const char* ESimTestBase::kTestEuiccPath = "/org/chromium/Hermes/Euicc/0";
const char* ESimTestBase::kTestEid = "12345678901234567890123456789012";

ESimTestBase::ESimTestBase()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  if (!ShillManagerClient::Get())
    shill_clients::InitializeFakes();
  if (!HermesManagerClient::Get())
    hermes_clients::InitializeFakes();
}

ESimTestBase::~ESimTestBase() = default;

void ESimTestBase::SetUp() {
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::Seconds(0));

  network_state_handler_ = NetworkStateHandler::InitializeForTest();
  network_device_handler_ =
      NetworkDeviceHandler::InitializeForTesting(network_state_handler_.get());
  network_configuration_handler_ =
      NetworkConfigurationHandler::InitializeForTest(
          network_state_handler_.get(), network_device_handler_.get());
  network_connection_handler_ =
      std::make_unique<FakeNetworkConnectionHandler>();
  network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
  cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
  cellular_inhibitor_->Init(network_state_handler_.get(),
                            network_device_handler_.get());
  cellular_esim_profile_handler_ =
      std::make_unique<TestCellularESimProfileHandler>();
  cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get());
  cellular_connection_handler_ = std::make_unique<CellularConnectionHandler>();
  cellular_connection_handler_->Init(network_state_handler_.get(),
                                     cellular_inhibitor_.get(),
                                     cellular_esim_profile_handler_.get());
  cellular_esim_installer_ = std::make_unique<CellularESimInstaller>();
  cellular_esim_installer_->Init(
      cellular_connection_handler_.get(), cellular_inhibitor_.get(),
      network_connection_handler_.get(), network_profile_handler_.get(),
      network_state_handler_.get());
  cellular_esim_uninstall_handler_ =
      std::make_unique<CellularESimUninstallHandler>();
  cellular_esim_uninstall_handler_->Init(
      cellular_inhibitor_.get(), cellular_esim_profile_handler_.get(),
      /*managed_cellular_pref_handler=*/nullptr,
      network_configuration_handler_.get(), network_connection_handler_.get(),
      network_state_handler_.get());

  esim_manager_ = std::make_unique<ESimManager>(
      cellular_connection_handler_.get(), cellular_esim_installer_.get(),
      cellular_esim_profile_handler_.get(),
      cellular_esim_uninstall_handler_.get(), cellular_inhibitor_.get(),
      network_connection_handler_.get(), network_state_handler_.get());
  observer_ = std::make_unique<ESimManagerTestObserver>();
  esim_manager_->AddObserver(observer_->GenerateRemote());
}

void ESimTestBase::TearDown() {
  esim_manager_.reset();
  observer_.reset();
  HermesEuiccClient::Get()->GetTestInterface()->ResetPendingEventsRequested();
}

void ESimTestBase::SetupEuicc() {
  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
      /*physical_slot=*/0);
  base::RunLoop().RunUntilIdle();
}

std::vector<mojo::PendingRemote<mojom::Euicc>>
ESimTestBase::GetAvailableEuiccs() {
  std::vector<mojo::PendingRemote<mojom::Euicc>> result;
  base::RunLoop run_loop;
  esim_manager()->GetAvailableEuiccs(base::BindOnce(
      [](std::vector<mojo::PendingRemote<mojom::Euicc>>* result,
         base::OnceClosure quit_closure,
         std::vector<mojo::PendingRemote<mojom::Euicc>> available_euiccs) {
        for (auto& euicc : available_euiccs)
          result->push_back(std::move(euicc));
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

mojo::Remote<mojom::Euicc> ESimTestBase::GetEuiccForEid(
    const std::string& eid) {
  std::vector<mojo::PendingRemote<mojom::Euicc>> euicc_pending_remotes =
      GetAvailableEuiccs();
  for (auto& euicc_pending_remote : euicc_pending_remotes) {
    mojo::Remote<mojom::Euicc> euicc(std::move(euicc_pending_remote));
    mojom::EuiccPropertiesPtr euicc_properties = GetEuiccProperties(euicc);
    if (euicc_properties->eid == eid) {
      return euicc;
    }
  }
  return mojo::Remote<mojom::Euicc>();
}

void ESimTestBase::FastForwardProfileRefreshDelay() {
  const base::TimeDelta kProfileRefreshCallbackDelay = base::Milliseconds(150);

  // Connect can result in two profile refresh calls before and after
  // enabling profile. Fast forward by delay after refresh.
  task_environment()->FastForwardBy(2 * kProfileRefreshCallbackDelay);
}

void ESimTestBase::FastForwardAutoConnectWaiting() {
  task_environment_.FastForwardBy(
      CellularConnectionHandler::kWaitingForAutoConnectTimeout);
}

}  // namespace ash::cellular_setup
