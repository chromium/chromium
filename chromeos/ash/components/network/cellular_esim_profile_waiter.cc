// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_waiter.h"

#include <string>
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace ash {

CellularESimProfileWaiter::CellularESimProfileWaiter() = default;

CellularESimProfileWaiter::~CellularESimProfileWaiter() {
  hermes_manager_client_observer_.Reset();
  hermes_profile_client_observer_.Reset();
  if (!on_success_.is_null()) {
    NET_LOG(EVENT)
        << "CellularESimProfileWaiter was destructed while waiting for "
        << "all of the conditions to be satisfied";
    std::move(on_success_).Run();
  }
  on_shutdown_.Reset();
}

void CellularESimProfileWaiter::RequirePendingProfile(
    const dbus::ObjectPath& profile_path) {
  AddCondition(
      profile_path,
      base::BindRepeating(
          [](HermesProfileClient::Properties* profile_properties) {
            return profile_properties &&
                   profile_properties->state().value() ==
                       hermes::profile::State::kPending &&
                   !profile_properties->name().value().empty() &&
                   !profile_properties->activation_code().value().empty();
          }));
}

void CellularESimProfileWaiter::Wait(base::OnceCallback<void()> on_success,
                                     base::OnceCallback<void()> on_shutdown) {
  DCHECK(on_success_.is_null() && !on_success.is_null());
  DCHECK(on_shutdown_.is_null() && !on_shutdown.is_null());
  on_success_ = std::move(on_success);
  on_shutdown_ = std::move(on_shutdown);

  if (profile_path_to_condition_.empty()) {
    std::move(on_success_).Run();
    on_shutdown_.Reset();
    return;
  }

  hermes_manager_client_observer_.Observe(HermesManagerClient::Get());
  hermes_profile_client_observer_.Observe(HermesProfileClient::Get());

  for (auto entry = profile_path_to_condition_.begin();
       entry != profile_path_to_condition_.end();) {
    if (EvaluateCondition(entry->first, entry->second)) {
      entry = profile_path_to_condition_.erase(entry);
    } else {
      entry++;
    }
  }
  MaybeFinish();
}

bool CellularESimProfileWaiter::waiting() const {
  return !profile_path_to_condition_.empty();
}

void CellularESimProfileWaiter::OnShutdown() {
  NET_LOG(EVENT) << "Hermes clients shut down while waiting for all of the "
                 << "conditions to be satisfied";

  hermes_manager_client_observer_.Reset();
  hermes_profile_client_observer_.Reset();

  profile_path_to_condition_.clear();

  DCHECK(!on_shutdown_.is_null());

  on_success_.Reset();
  std::move(on_shutdown_).Run();
}

void CellularESimProfileWaiter::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  auto entry = profile_path_to_condition_.find(object_path);
  if (entry == profile_path_to_condition_.end()) {
    return;
  }
  if (EvaluateCondition(/*profile_path=*/entry->first,
                        /*condition=*/entry->second)) {
    profile_path_to_condition_.erase(entry->first);
  }
  MaybeFinish();
}

void CellularESimProfileWaiter::AddCondition(
    const dbus::ObjectPath& profile_path,
    Condition condition) {
  DCHECK(!condition.is_null());
  profile_path_to_condition_.insert_or_assign(profile_path,
                                              std::move(condition));
}

bool CellularESimProfileWaiter::EvaluateCondition(
    const dbus::ObjectPath& profile_path,
    const Condition& condition) {
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  if (!profile_properties) {
    return false;
  }
  return condition.Run(profile_properties);
}

void CellularESimProfileWaiter::MaybeFinish() {
  if (!profile_path_to_condition_.empty()) {
    return;
  }
  hermes_manager_client_observer_.Reset();
  hermes_profile_client_observer_.Reset();

  if (on_success_.is_null()) {
    return;
  }

  NET_LOG(EVENT) << "All conditions have been satisfied";

  std::move(on_success_).Run();
  on_shutdown_.Reset();
}

}  // namespace ash
