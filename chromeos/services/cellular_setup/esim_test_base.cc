// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_test_base.h"

#include <memory.h>

#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/services/cellular_setup/esim_manager.h"
#include "chromeos/services/cellular_setup/esim_test_utils.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"

namespace chromeos {
namespace cellular_setup {

const char* ESimTestBase::kTestEuiccPath = "/org/chromium/Hermes/Euicc/0";
const char* ESimTestBase::kTestEid = "12345678901234567890123456789012";

ESimTestBase::ESimTestBase() {
  if (!ShillManagerClient::Get())
    shill_clients::InitializeFakes();
  if (!HermesManagerClient::Get())
    hermes_clients::InitializeFakes();
}

ESimTestBase::~ESimTestBase() = default;

void ESimTestBase::SetUp() {
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      base::TimeDelta::FromSeconds(0));
  esim_manager_ = std::make_unique<ESimManager>();
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
      dbus::ObjectPath(kTestEuiccPath), kTestEid, true);
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

}  // namespace cellular_setup
}  // namespace chromeos