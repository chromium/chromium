// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_NET_IN_PROCESS_NETWORK_CONNECTION_TRACKER_H_
#define CHROME_SERVICE_NET_IN_PROCESS_NETWORK_CONNECTION_TRACKER_H_

#include "base/macros.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"

// A NetworkConnectionTracker subclass that directly wraps the
// NetworkChangeNotifier instead of getting connection notifications via
// the NetworkChangeManager mojo service.
//
// This is needed because some dependencies need a NetworkConnectionTracker, but
// we can't start up the network service within the Cloud Print service process.
class InProcessNetworkConnectionTracker
    : public network::NetworkConnectionTracker,
      private net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  InProcessNetworkConnectionTracker();
  ~InProcessNetworkConnectionTracker() override;

 protected:
  using network::NetworkConnectionTracker::OnNetworkChanged;

 private:
  // net::NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  DISALLOW_COPY_AND_ASSIGN(InProcessNetworkConnectionTracker);
};

#endif  // CHROME_SERVICE_NET_IN_PROCESS_NETWORK_CONNECTION_TRACKER_H_
