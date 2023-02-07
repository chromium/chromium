// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_metadata_observer.h"

#include <string>

#include "base/values.h"

namespace ash {

NetworkMetadataObserver::NetworkMetadataObserver() = default;

NetworkMetadataObserver::~NetworkMetadataObserver() = default;

void NetworkMetadataObserver::OnFirstConnectionToNetwork(
    const std::string& guid) {}

void NetworkMetadataObserver::OnNetworkCreated(const std::string& guid) {}

void NetworkMetadataObserver::OnNetworkUpdate(
    const std::string& guid,
    const base::Value::Dict* set_properties) {}

}  // namespace ash
