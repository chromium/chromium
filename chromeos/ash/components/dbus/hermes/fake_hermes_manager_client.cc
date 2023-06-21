// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/fake_hermes_manager_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {
const char* kDefaultEuiccPath = "/org/chromium/Hermes/Euicc/0";
const char* kDefaultEid = "12345678901234567890123456789012";
}  // namespace

FakeHermesManagerClient::FakeHermesManagerClient() {
  ParseCommandLineSwitch();
}

FakeHermesManagerClient::~FakeHermesManagerClient() = default;

void FakeHermesManagerClient::AddEuicc(const dbus::ObjectPath& path,
                                       const std::string& eid,
                                       bool is_active,
                                       uint32_t physical_slot) {
  DVLOG(1) << "Adding new euicc path=" << path.value() << ", eid=" << eid
           << ", active=" << is_active;
  HermesEuiccClient::Properties* properties =
      HermesEuiccClient::Get()->GetProperties(path);
  properties->eid().ReplaceValue(eid);
  properties->is_active().ReplaceValue(is_active);
  properties->physical_slot().ReplaceValue(physical_slot);
  available_euiccs_.push_back(path);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesManagerClient::NotifyAvailableEuiccListChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeHermesManagerClient::ClearEuiccs() {
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  for (const auto& path : available_euiccs_) {
    euicc_test->ClearEuicc(path);
  }
  available_euiccs_.clear();
}

const std::vector<dbus::ObjectPath>&
FakeHermesManagerClient::GetAvailableEuiccs() {
  return available_euiccs_;
}

HermesManagerClient::TestInterface*
FakeHermesManagerClient::GetTestInterface() {
  return this;
}

void FakeHermesManagerClient::ParseCommandLineSwitch() {
  // Parse hermes stub commandline switch. Stubs are setup only if a value
  // of "on" is passed.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kHermesFake))
    return;
  std::string switch_value =
      command_line->GetSwitchValueASCII(chromeos::switches::kHermesFake);
  if (switch_value != "on")
    return;

  // Add a default Euicc and an installed fake carrier profile
  // as initial environment.
  AddEuicc(dbus::ObjectPath(kDefaultEuiccPath), kDefaultEid, /*is_active=*/true,
           /*physical_slot=*/0);
  HermesEuiccClient::TestInterface* euicc_client_test =
      HermesEuiccClient::Get()->GetTestInterface();
  euicc_client_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kDefaultEuiccPath), hermes::profile::State::kInactive,
      /*activation_code=*/std::string(),
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddDelayedProfileWithService);
}

void FakeHermesManagerClient::NotifyAvailableEuiccListChanged() {
  HermesEuiccClient::TestInterface* euicc_client_test =
      HermesEuiccClient::Get()->GetTestInterface();
  euicc_client_test->UpdateShillDeviceSimSlotInfo();
  for (auto& observer : observers()) {
    observer.OnAvailableEuiccListChanged();
  }
}

}  // namespace ash
