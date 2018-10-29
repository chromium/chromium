// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/in_process_network_connection_tracker.h"

InProcessNetworkConnectionTracker::InProcessNetworkConnectionTracker() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  OnInitialConnectionType(network::mojom::ConnectionType(
      net::NetworkChangeNotifier::GetConnectionType()));
}

InProcessNetworkConnectionTracker::~InProcessNetworkConnectionTracker() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void InProcessNetworkConnectionTracker::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  network::NetworkConnectionTracker::OnNetworkChanged(
      network::mojom::ConnectionType(type));
}
