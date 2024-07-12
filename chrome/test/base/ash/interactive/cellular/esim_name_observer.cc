// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/cellular/esim_name_observer.h"

#include "base/logging.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

EsimNameObserver::EsimNameObserver(const dbus::ObjectPath object_path)
    : ObservationStateObserver(HermesProfileClient::Get()),
      object_path_(object_path) {}

EsimNameObserver::~EsimNameObserver() = default;

void EsimNameObserver::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_ ||
      property_name != hermes::profile::kNicknameProperty) {
    return;
  }

  OnStateObserverStateChanged(GetNetworkDisplayName());
}

std::string EsimNameObserver::GetStateObserverInitialState() const {
  return GetNetworkDisplayName();
}

std::string EsimNameObserver::GetNetworkDisplayName() const {
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(object_path_);
  CHECK(profile_properties);

  const std::string& nickname = profile_properties->nick_name().value();
  return !nickname.empty() ? nickname : profile_properties->name().value();
}

}  // namespace ash
