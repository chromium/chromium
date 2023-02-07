// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_configuration_observer.h"

namespace ash {

// NetworkConfigurationObserver::NetworkConfigurationObserver() = default;

NetworkConfigurationObserver::~NetworkConfigurationObserver() = default;

void NetworkConfigurationObserver::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {}

void NetworkConfigurationObserver::OnConfigurationModified(
    const std::string& service_path,
    const std::string& guid,
    const base::Value::Dict* set_properties) {}

void NetworkConfigurationObserver::OnBeforeConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {}

void NetworkConfigurationObserver::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {}

void NetworkConfigurationObserver::OnShuttingDown() {}

}  // namespace ash
