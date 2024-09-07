// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/cellular_util.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/ash/interactive/network/shill_service_util.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

EuiccInfo::EuiccInfo(unsigned int id)
    : path_(base::StringPrintf("/hermes/euicc%u", id)),
      eid_(base::StringPrintf("%032u", id)) {}

EuiccInfo::~EuiccInfo() = default;

SimInfo::SimInfo(unsigned int id)
    : guid_(base::StringPrintf("esim_guid%018u", id)),
      profile_path_(base::StringPrintf("/profile/path%u", id)),
      iccid_(base::StringPrintf("%018u", id)),
      name_(base::StringPrintf("Profile Name %u", id)),
      nickname_(base::StringPrintf("Profile Nickname %u", id)),
      service_provider_(base::StringPrintf("Service Provider %u", id)),
      // TODO(b/339260115): Align Shill and Hermes fakes so that the service
      // path and GUID logic is consistent for cellular networks.
      service_path_(base::StringPrintf("service_path_for_%s", iccid_.c_str())) {
  auto* hermes_euicc_client = HermesEuiccClient::Get()->GetTestInterface();
  CHECK(hermes_euicc_client);
  activation_code_ = hermes_euicc_client->GenerateFakeActivationCode();
}

SimInfo::~SimInfo() = default;

void SimInfo::Connect() const {
  ConnectShillService(service_path_);
}

void SimInfo::Disconnect() const {
  DisconnectShillService(service_path_);
}

void ConfigureEsimProfile(const EuiccInfo& euicc_info,
                          const SimInfo& esim_info,
                          bool connected) {
  auto* hermes_euicc_client = HermesEuiccClient::Get()->GetTestInterface();
  CHECK(hermes_euicc_client);

  // Add an inactive profile.
  hermes_euicc_client->AddCarrierProfile(
      dbus::ObjectPath(esim_info.profile_path()),
      dbus::ObjectPath(euicc_info.path()), esim_info.iccid(), esim_info.name(),
      esim_info.nickname(), esim_info.service_provider(),
      esim_info.activation_code(),
      /*network_service_path=*/esim_info.service_path(),
      /*state=*/hermes::profile::State::kInactive,
      /*profile_class=*/hermes::profile::ProfileClass::kOperational,
      /*add_carrier_profile_behavior=*/
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);

  if (connected) {
    // Explicitly enable the newly added profile.
    auto* hermes_profile_client = HermesProfileClient::Get();
    CHECK(hermes_profile_client);
    hermes_profile_client->EnableCarrierProfile(
        dbus::ObjectPath(esim_info.profile_path()), base::DoNothing());

    esim_info.Connect();
  }
}

}  // namespace ash
