// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_connection_observer.h"

namespace ash {

NetworkConnectionObserver::NetworkConnectionObserver() = default;

void NetworkConnectionObserver::ConnectToNetworkRequested(
    const std::string& service_path) {
}

void NetworkConnectionObserver::ConnectSucceeded(
    const std::string& service_path) {
}

void NetworkConnectionObserver::ConnectFailed(const std::string& service_path,
                                              const std::string& error_name) {
}

void NetworkConnectionObserver::DisconnectRequested(
    const std::string& service_path) {
}

NetworkConnectionObserver::~NetworkConnectionObserver() = default;

}  // namespace ash
